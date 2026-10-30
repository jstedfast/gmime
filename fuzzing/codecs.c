/* Adapted from MimeKit.Fuzzing.Targets.{Encoders,Decoders}Target.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

#define GUARD_LENGTH 64
#define GUARD 0xcc

static guint8 *
guarded_buffer (gsize capacity)
{
	guint8 *buffer;

	FUZZ_CHECK (capacity <= 4 * FUZZ_MAX_LENGTH + 4096, "Unreasonable codec output estimate");
	buffer = g_malloc (capacity + 2 * GUARD_LENGTH);
	memset (buffer, GUARD, capacity + 2 * GUARD_LENGTH);
	return buffer;
}

static void
check_guard (const guint8 *buffer, gsize capacity, gsize written, const char *name)
{
	gsize i;

	FUZZ_CHECK (written <= capacity, "%s exceeded its output estimate", name);
	for (i = 0; i < GUARD_LENGTH; i++)
		FUZZ_CHECK (buffer[i] == GUARD, "%s wrote before its output buffer", name);
	/* GMime promises exact stores, including on SIMD paths. */
	for (i = GUARD_LENGTH + written; i < GUARD_LENGTH + capacity + GUARD_LENGTH; i++)
		FUZZ_CHECK (buffer[i] == GUARD, "%s wrote beyond its returned output length", name);
}

static void
encode (GMimeEncoder *encoder, const guint8 *input, gsize length, gboolean flush, GByteArray *result)
{
	gsize capacity = g_mime_encoder_estimate_output_length (encoder, length);
	guint8 *buffer = guarded_buffer (capacity);
	gsize written;

	written = flush ? g_mime_encoder_flush (encoder, input, length, buffer + GUARD_LENGTH, capacity) :
		g_mime_encoder_encode (encoder, input, length, buffer + GUARD_LENGTH, capacity);
	check_guard (buffer, capacity, written, G_OBJECT_TYPE_NAME (encoder));
	g_byte_array_append (result, buffer + GUARD_LENGTH, written);
	g_free (buffer);
}

static void
decode (GMimeDecoder *decoder, const guint8 *input, gsize length, GByteArray *result)
{
	gsize capacity = g_mime_decoder_estimate_output_length (decoder, length);
	guint8 *buffer = guarded_buffer (capacity);
	gsize written = g_mime_decoder_decode (decoder, input, length, buffer + GUARD_LENGTH, capacity);

	check_guard (buffer, capacity, written, G_OBJECT_TYPE_NAME (decoder));
	g_byte_array_append (result, buffer + GUARD_LENGTH, written);
	g_free (buffer);
}

static void
equal (GByteArray *expected, GByteArray *actual, const char *what)
{
	fuzz_equal (expected->data, expected->len, actual->data, actual->len, what);
}

static GMimeEncoder *
new_encoder (guint kind, guint option)
{
	GMimeNewLineFormat format = option & 128 ? GMIME_NEWLINE_FORMAT_UNIX : GMIME_NEWLINE_FORMAT_DOS;
	guint width = 60 + (option * 4 % 939);

	switch (kind) {
	case 0: return GMIME_ENCODER (g_mime_base64_encoder_new_with_newline_format (format, 60 + option % 17));
	case 1: return GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_newline_format (format, width));
	case 2: return GMIME_ENCODER (g_mime_uu_encoder_new_with_newline_format (format));
	case 3: return GMIME_ENCODER (g_mime_yenc_encoder_new_with_newline_format (format, width));
	default: return GMIME_ENCODER (g_mime_pass_through_encoder_new (
		option & 1 ? GMIME_CONTENT_ENCODING_BINARY : GMIME_CONTENT_ENCODING_8BIT));
	}
}

static GMimeDecoder *
new_decoder (guint kind)
{
	switch (kind) {
	case 0: return GMIME_DECODER (g_mime_base64_decoder_new ());
	case 1: return GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
	case 2: return GMIME_DECODER (g_mime_quoted_printable_decoder_new (TRUE));
	case 3: return GMIME_DECODER (g_mime_uu_decoder_new (FALSE));
	case 4: return GMIME_DECODER (g_mime_uu_decoder_new (TRUE));
	case 5: return GMIME_DECODER (g_mime_yenc_decoder_new (FALSE));
	case 6: return GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
	default: return GMIME_DECODER (g_mime_pass_through_decoder_new (GMIME_CONTENT_ENCODING_BINARY));
	}
}

static GMimeDecoder *
matching_decoder (guint kind)
{
	static const guint kinds[] = { 0, 1, 4, 6, 7 };

	return new_decoder (kinds[kind]);
}

static GByteArray *
expected_input (guint kind, guint option, const guint8 *input, gsize length)
{
	GByteArray *expected = g_byte_array_new ();
	gsize i;

	for (i = 0; i < length; i++) {
		if (kind == 1) {
			if (input[i] == '\r' && i + 1 < length && input[i + 1] == '\n')
				continue;
			if (input[i] == '\n' && !(option & 128))
				g_byte_array_append (expected, (const guint8 *) "\r", 1);
		}
		g_byte_array_append (expected, input + i, 1);
	}
	return expected;
}

static guint32
crc32_reference (const guint8 *input, gsize length)
{
	guint32 crc = 0xffffffff;
	gsize i;
	guint bit;

	for (i = 0; i < length; i++) {
		crc ^= input[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return ~crc;
}

static gsize
next_chunk (guint32 *state, gsize remaining)
{
	static const guint sizes[] = { 1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 57, 76, 77, 78, 511, 4096 };

	*state = *state * 1664525u + 1013904223u;
	return MIN (sizes[(*state >> 24) % G_N_ELEMENTS (sizes)], remaining);
}

static void
check_lines (guint kind, guint option, const guint8 *input, gsize length, GByteArray *encoded)
{
	GByteArray *normalized = g_byte_array_new ();
	gsize i, j, start = 0, total = 0, count, width = 60 + (option * 4 % 939);
	guint32 bits;
	guint shift, sextet;
	gboolean ended = FALSE;
	guint8 c;

	if (kind == 4) {
		fuzz_equal (input, length, encoded->data, encoded->len, "Pass-through encoding");
		g_byte_array_unref (normalized);
		return;
	}
	for (i = 0; i < encoded->len; i++) {
		c = encoded->data[i];
		if (c == '\r') {
			FUZZ_CHECK (!(option & 128) && i + 1 < encoded->len && encoded->data[i + 1] == '\n',
				    "Encoder emitted an unexpected CR");
			continue;
		}
		if (c == '\n' && !(option & 128))
			FUZZ_CHECK (i && encoded->data[i - 1] == '\r', "Encoder emitted a bare LF");
		g_byte_array_append (normalized, &c, 1);
	}
	for (i = 0; i < normalized->len; i++) {
		c = normalized->data[i];
		if (c == '\n') {
			count = i - start;
			switch (kind) {
			case 0:
				width = (60 + option % 17) / 4 * 4;
				FUZZ_CHECK (count && count % 4 == 0 && count <= width &&
					    (i + 1 == normalized->len || count == width), "Invalid Base64 line length");
				break;
			case 1:
				FUZZ_CHECK (count <= MIN (width, 76) &&
					    (!count || (normalized->data[i - 1] != ' ' && normalized->data[i - 1] != '\t')),
					    "Invalid quoted-printable line");
				break;
			case 2:
				FUZZ_CHECK (!ended && count, "Invalid uuencode termination");
				count = (normalized->data[start] - 32) & 63;
				FUZZ_CHECK (count <= 45 && i - start - 1 == (count + 2) / 3 * 4, "Invalid uuencode count");
				FUZZ_CHECK (total + count <= length, "uuencode count exceeds input");
				for (j = 0; j < count; j += 3) {
					bits = (guint32) input[total + j] << 16;
					if (j + 1 < count)
						bits |= (guint32) input[total + j + 1] << 8;
					if (j + 2 < count)
						bits |= input[total + j + 2];
					for (shift = 0; shift < 4; shift++) {
						sextet = (bits >> (18 - shift * 6)) & 63;
						FUZZ_CHECK (normalized->data[start + 1 + j / 3 * 4 + shift] ==
							    (sextet ? sextet + 32 : 96), "uuencode differs from scalar reference");
					}
				}
				if (!count)
					ended = TRUE;
				else if (total + count < length)
					FUZZ_CHECK (count == 45, "Premature short uuencode line");
				total += count;
				break;
			case 3:
				FUZZ_CHECK (count && count <= width + 1, "Invalid yEnc line length");
				break;
			}
			start = i + 1;
		} else if (kind == 1) {
			if (c == '=')
				FUZZ_CHECK ((i + 1 < normalized->len && normalized->data[i + 1] == '\n') ||
					    (i + 2 < normalized->len && g_ascii_isxdigit (normalized->data[i + 1]) &&
					     g_ascii_isxdigit (normalized->data[i + 2])), "Invalid quoted-printable escape");
			else
				FUZZ_CHECK (c == ' ' || c == '\t' || (c >= 33 && c <= 126), "Invalid quoted-printable byte");
		} else if (kind == 2) {
			FUZZ_CHECK (c >= 33 && c <= 96, "Invalid uuencode alphabet");
		} else if (kind == 3) {
			FUZZ_CHECK (c != 0, "Invalid yEnc alphabet");
		}
	}
	FUZZ_CHECK (start == normalized->len, "Encoder output lacks a final newline");
	if (kind == 2)
		FUZZ_CHECK (ended && total == length, "Invalid uuencode total length");
	if (kind == 0) {
		char *reference = g_base64_encode (input, length);
		GByteArray *unwrapped = g_byte_array_new ();

		for (i = 0; i < normalized->len; i++)
			if (normalized->data[i] != '\n')
				g_byte_array_append (unwrapped, normalized->data + i, 1);
		fuzz_equal ((const guint8 *) reference, strlen (reference), unwrapped->data, unwrapped->len,
			    "Base64 vs GLib reference");
		g_byte_array_unref (unwrapped);
		g_free (reference);
	}
	g_byte_array_unref (normalized);
}

static void
filter_roundtrip (GMimeEncoder *encoder, guint kind, const guint8 *input, gsize length,
		  GByteArray *encoded, GByteArray *expected, guint32 state)
{
	GMimeStream *memory = g_mime_stream_mem_new ();
	GMimeStream *stream = g_mime_stream_filter_new (memory);
	GMimeFilter *filter;
	GMimeDecoder *decoder = matching_decoder (kind);
	GByteArray *decoded = g_byte_array_new ();
	guint8 buffer[4096];
	gsize offset, count;
	ssize_t n;

	g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), FALSE);
	g_mime_encoder_reset (encoder);
	filter = g_mime_filter_basic_new_with_encoder (encoder);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
	g_object_unref (filter);
	for (offset = 0; offset < length; offset += count) {
		count = next_chunk (&state, length - offset);
		FUZZ_CHECK (g_mime_stream_write (stream, (const char *) input + offset, count) == (ssize_t) count,
			    "Encoder filter write failed");
	}
	FUZZ_CHECK (g_mime_stream_flush (stream) == 0, "Encoder filter flush failed");
	equal (encoded, g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory)), "Encoder filter vs direct encoding");
	g_object_unref (stream);
	FUZZ_CHECK (g_mime_stream_reset (memory) == 0, "Filter stream reset failed");
	stream = g_mime_stream_filter_new (memory);
	g_mime_stream_filter_set_owner (GMIME_STREAM_FILTER (stream), FALSE);
	filter = g_mime_filter_basic_new_with_decoder (decoder);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
	g_object_unref (filter);
	do {
		count = next_chunk (&state, sizeof (buffer));
		n = g_mime_stream_read (stream, (char *) buffer, count);
		FUZZ_CHECK (n >= 0, "Decoder filter read failed");
		g_byte_array_append (decoded, buffer, n);
	} while (n);
	equal (expected, decoded, "Decoder filter round trip");
	g_byte_array_unref (decoded);
	g_object_unref (decoder);
	g_object_unref (stream);
	g_object_unref (memory);
}

void
fuzz_encoders (const guint8 *data, gsize length)
{
	GMimeEncoder *encoder, *clone;
	GMimeDecoder *decoder;
	GByteArray *whole, *again, *chunked, *cloned, *scalar, *expected, *decoded;
	gsize split, offset, count;
	guint kind, option;
	guint32 state;

	if (length < 2)
		return;
	kind = data[0] % 5;
	option = data[1];
	state = fuzz_selector (data, length);
	data += 2;
	length -= 2;
	split = state % (length + 1);
	encoder = new_encoder (kind, option);
	whole = g_byte_array_new ();
	again = g_byte_array_new ();
	chunked = g_byte_array_new ();
	cloned = g_byte_array_new ();
	scalar = g_byte_array_new ();
	decoded = g_byte_array_new ();
	expected = expected_input (kind, option, data, length);
	encode (encoder, data, length, TRUE, whole);
	if (kind == 3)
		FUZZ_CHECK (~g_mime_yenc_encoder_get_checksum (GMIME_YENC_ENCODER (encoder)) == crc32_reference (data, length),
			    "yEnc checksum differs from reference CRC-32");
	else {
		encode (encoder, data, length, TRUE, again);
		equal (whole, again, "Encoding after flush");
	}
	g_byte_array_set_size (again, 0);
	g_mime_encoder_reset (encoder);
	encode (encoder, data, length, TRUE, again);
	equal (whole, again, "Encoding after reset");
	g_mime_encoder_reset (encoder);
	encode (encoder, data, split, FALSE, chunked);
	clone = g_mime_encoder_clone (encoder);
	g_byte_array_append (cloned, chunked->data, chunked->len);
	for (offset = split; offset < length; offset += count) {
		encode (encoder, NULL, 0, FALSE, chunked);
		encode (clone, NULL, 0, FALSE, cloned);
		count = next_chunk (&state, length - offset);
		encode (encoder, data + offset, count, FALSE, chunked);
		encode (clone, data + offset, count, FALSE, cloned);
	}
	encode (encoder, NULL, 0, TRUE, chunked);
	encode (clone, NULL, 0, TRUE, cloned);
	equal (whole, chunked, "Whole vs chunked encoding");
	equal (chunked, cloned, "Cloned encoder");
	g_mime_encoder_reset (encoder);
	/* This forces Base64's scalar path. UU buffers full lines, so its SIMD
	 * output is checked against an independent sextet reference above. */
	for (offset = 0; offset < length; offset += count) {
		count = MIN (7, length - offset);
		encode (encoder, data + offset, count, FALSE, scalar);
	}
	encode (encoder, NULL, 0, TRUE, scalar);
	equal (whole, scalar, "Whole vs seven-byte encoding");
	check_lines (kind, option, data, length, whole);
	decoder = matching_decoder (kind);
	decode (decoder, whole->data, whole->len, decoded);
	equal (expected, decoded, "Encoder/decoder round trip");
	filter_roundtrip (encoder, kind, data, length, whole, expected, state);
	g_object_unref (decoder);
	g_object_unref (clone);
	g_object_unref (encoder);
	g_byte_array_unref (whole);
	g_byte_array_unref (again);
	g_byte_array_unref (chunked);
	g_byte_array_unref (cloned);
	g_byte_array_unref (scalar);
	g_byte_array_unref (expected);
	g_byte_array_unref (decoded);
}

void
fuzz_decoders (const guint8 *data, gsize length)
{
	GMimeDecoder *decoder, *clone;
	GByteArray *whole, *again, *chunked, *tail, *cloned, *scalar, *encoded, *expected;
	GMimeEncoder *encoder;
	guint selector, kind;
	gsize split, offset, count;

	if (!length)
		return;
	selector = *data++;
	length--;
	split = (selector >> 3) * length / 31;
	decoder = new_decoder (selector & 7);
	whole = g_byte_array_new ();
	again = g_byte_array_new ();
	chunked = g_byte_array_new ();
	tail = g_byte_array_new ();
	cloned = g_byte_array_new ();
	scalar = g_byte_array_new ();
	decode (decoder, data, length, whole);
	g_mime_decoder_reset (decoder);
	decode (decoder, data, length, again);
	equal (whole, again, "Decoding after reset");
	g_mime_decoder_reset (decoder);
	decode (decoder, data, split, chunked);
	clone = g_mime_decoder_clone (decoder);
	decode (decoder, NULL, 0, tail);
	decode (decoder, data + split, length - split, tail);
	decode (clone, data + split, length - split, cloned);
	equal (tail, cloned, "Cloned decoder");
	g_byte_array_append (chunked, tail->data, tail->len);
	equal (whole, chunked, "Whole vs split decoding");
	g_mime_decoder_reset (decoder);
	for (offset = 0; offset < length; offset += count) {
		count = MIN (7, length - offset);
		decode (decoder, data + offset, count, scalar);
	}
	equal (whole, scalar, "SIMD vs scalar-sized decoding");
	g_object_unref (decoder);
	g_object_unref (clone);
	for (kind = 0; kind < 4; kind++) {
		encoder = new_encoder (kind, selector);
		decoder = matching_decoder (kind);
		encoded = g_byte_array_new ();
		expected = expected_input (kind, selector, data, length);
		encode (encoder, data, split, FALSE, encoded);
		encode (encoder, data + split, length - split, TRUE, encoded);
		g_byte_array_set_size (again, 0);
		decode (decoder, encoded->data, encoded->len, again);
		equal (expected, again, "Split encode/flush round trip");
		g_byte_array_unref (expected);
		g_byte_array_unref (encoded);
		g_object_unref (decoder);
		g_object_unref (encoder);
	}
	g_byte_array_unref (whole);
	g_byte_array_unref (again);
	g_byte_array_unref (chunked);
	g_byte_array_unref (tail);
	g_byte_array_unref (cloned);
	g_byte_array_unref (scalar);
}
