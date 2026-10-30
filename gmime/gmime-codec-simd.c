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

/*
 * SIMD codec and validator primitives ported from MimeKit. The Base64 kernels
 * derive from .NET Core's Base64EncoderHelper/Base64DecoderHelper and the
 * base64simd algorithms of Wojciech Mula. The AVX2 MIME-line tail fast path
 * follows MimeKit's Base64Decoder optimization.
 *
 * Portions adapted from MimeKit:
 * Copyright (c) 2013-2026 .NET Foundation and Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "gmime-codec-private.h"

static gsize
scan_base64_scalar (const guint8 *input, gsize length, GMimeCodecScan *scan)
{
	gsize i;
	guint8 c;

	for (i = 0; i < length; i++) {
		c = input[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '+' || c == '/') {
			scan->alphabet_count++;
		} else if (c == '\n') {
			scan->line_feed_count++;
			scan->line_begin_offset = i + 1;
		} else if (c != '\r' && c != ' ' && c != '\t') {
			break;
		}
	}

	return i;
}

static gsize
index_of_qp_special_scalar (const guint8 *input, gsize length)
{
	gsize i;

	for (i = 0; i < length; i++) {
		if (input[i] == '=' || input[i] == '\n')
			break;
	}

	return i;
}

#if !defined(GMIME_DISABLE_SIMD) && \
    (((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) || \
     (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))))
#define HAVE_SSSE3_CODEC 1
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#define SSSE3_TARGET
#define AVX2_TARGET
#else
#define SSSE3_TARGET __attribute__((target("ssse3")))
#define AVX2_TARGET __attribute__((target("avx2")))
#endif

static guint
mask_popcount (guint mask)
{
	mask -= (mask >> 1) & 0x55555555;
	mask = (mask & 0x33333333) + ((mask >> 2) & 0x33333333);
	return (((mask + (mask >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

static SSSE3_TARGET gsize
scan_base64_ssse3 (const guint8 *input, gsize length, GMimeCodecScan *scan)
{
	gsize consumed = 0, tail_length;
	GMimeCodecScan tail = { 0 };
	guint alphabet_mask, line_feed_mask, valid, count, prefix;
	__m128i block, letters, digits, alphabet, line_feed, whitespace;

	while (length - consumed >= 16) {
		block = _mm_loadu_si128 ((const __m128i *) (input + consumed));
		/* Bias the unsigned ranges to signed-byte minima before comparing. */
		letters = _mm_cmpgt_epi8 (_mm_set1_epi8 (-128 + 26),
					 _mm_add_epi8 (_mm_or_si128 (block, _mm_set1_epi8 (0x20)),
						       _mm_set1_epi8 (0x80 - 'a')));
		digits = _mm_cmpgt_epi8 (_mm_set1_epi8 (-128 + 10),
					_mm_add_epi8 (block, _mm_set1_epi8 (0x80 - '0')));
		alphabet = _mm_or_si128 (_mm_or_si128 (letters, digits),
					_mm_or_si128 (_mm_cmpeq_epi8 (block, _mm_set1_epi8 ('+')),
						      _mm_cmpeq_epi8 (block, _mm_set1_epi8 ('/'))));
		alphabet_mask = (guint) _mm_movemask_epi8 (alphabet);
		if (alphabet_mask == 0xffff) {
			scan->alphabet_count += 16;
			consumed += 16;
			continue;
		}

		line_feed = _mm_cmpeq_epi8 (block, _mm_set1_epi8 ('\n'));
		whitespace = _mm_or_si128 (_mm_cmpeq_epi8 (block, _mm_set1_epi8 ('\r')),
					  _mm_or_si128 (_mm_cmpeq_epi8 (block, _mm_set1_epi8 (' ')),
							_mm_cmpeq_epi8 (block, _mm_set1_epi8 ('\t'))));
		valid = (guint) _mm_movemask_epi8 (_mm_or_si128 (alphabet, _mm_or_si128 (line_feed, whitespace)));
		/* Consume only the prefix before padding, a comment marker or invalid input;
		 * the scalar validator must inspect the first special byte itself. */
		count = valid == 0xffff ? 16 : (guint) g_bit_nth_lsf (~valid & 0xffff, -1);
		prefix = (1u << count) - 1;
		alphabet_mask &= prefix;
		line_feed_mask = (guint) _mm_movemask_epi8 (line_feed) & prefix;
		scan->alphabet_count += mask_popcount (alphabet_mask);
		if (line_feed_mask) {
			scan->line_feed_count += mask_popcount (line_feed_mask);
			scan->line_begin_offset = consumed + g_bit_storage (line_feed_mask);
		}
		consumed += count;
		if (count < 16)
			return consumed;
	}

	if (consumed < length) {
		tail_length = scan_base64_scalar (input + consumed, length - consumed, &tail);
		scan->alphabet_count += tail.alphabet_count;
		scan->line_feed_count += tail.line_feed_count;
		if (tail.line_feed_count)
			scan->line_begin_offset = consumed + tail.line_begin_offset;
		consumed += tail_length;
	}

	return consumed;
}

static SSSE3_TARGET gsize
index_of_qp_special_ssse3 (const guint8 *input, gsize length)
{
	gsize consumed = 0;
	__m128i block, special;
	guint mask;

	while (length - consumed >= 16) {
		block = _mm_loadu_si128 ((const __m128i *) (input + consumed));
		special = _mm_or_si128 (_mm_cmpeq_epi8 (block, _mm_set1_epi8 ('=')),
				       _mm_cmpeq_epi8 (block, _mm_set1_epi8 ('\n')));
		mask = (guint) _mm_movemask_epi8 (special);
		if (mask)
			return consumed + g_bit_nth_lsf (mask, -1);
		consumed += 16;
	}

	if (consumed < length)
		consumed += index_of_qp_special_scalar (input + consumed, length - consumed);

	return consumed;
}

static SSSE3_TARGET gsize
encode_blocks_ssse3 (const guint8 *input, gsize length, guint8 *output, guint quartets, gboolean uu)
{
	const __m128i shuffle = _mm_set_epi32 (0x0a0b090a, 0x07080607, 0x04050304, 0x01020001);
	const __m128i lut = _mm_set_epi32 (0x0000f0ed, 0xfcfcfcfc, 0xfcfcfcfc, 0xfcfc4741);
	gsize consumed = 0;
	__m128i str, a, b, indices;

	/* Each vector consumes 12 input bytes but loads 16. Require a full readable
	 * vector and four quartets of line space; leave the tail to the scalar encoder. */
	while (length >= 16 && quartets >= 4) {
		str = _mm_loadu_si128 ((const __m128i *) input);
		/* Reshuffle each input triplet a,b,c into b,c,a,b (MSB to LSB).
		 * Masks and 16-bit multiplies then redistribute its bits into four sextets:
		 *   0000bbbb CC000000 AAAAAA00 00000000
		 *   00000000 00cccccc 000000aa BBBB0000
		 * yielding:
		 *   00cccccc 00bbbbCC 00aaBBBB 00AAAAAA */
		str = _mm_shuffle_epi8 (str, shuffle);
		a = _mm_mulhi_epu16 (_mm_and_si128 (str, _mm_set1_epi32 (0x0fc0fc00)), _mm_set1_epi32 (0x04000040));
		b = _mm_mullo_epi16 (_mm_and_si128 (str, _mm_set1_epi32 (0x003f03f0)), _mm_set1_epi32 (0x01000010));
		str = _mm_or_si128 (a, b);
		if (uu) {
			/* UU maps zero to '`' (0x60), and 1..63 to 0x21..0x5f. */
			a = _mm_cmpeq_epi8 (str, _mm_setzero_si128 ());
			str = _mm_add_epi8 (str, _mm_set1_epi8 (32));
			str = _mm_add_epi8 (str, _mm_and_si128 (a, _mm_set1_epi8 (64)));
		} else {
			/* Translate sextets by adding the offset selected from lut:
			 *   values    offset   lut index   alphabet
			 *    0..25      +65        0        A..Z
			 *   26..51      +71        1        a..z
			 *   52..61       -4       2..11     0..9
			 *       62      -19       12        +
			 *       63      -16       13        /
			 * Saturating subtraction leaves non-uppercase indices one too small;
			 * subtracting the comparison mask (-1) corrects them. */
			indices = _mm_subs_epu8 (str, _mm_set1_epi8 (51));
			indices = _mm_sub_epi8 (indices, _mm_cmpgt_epi8 (str, _mm_set1_epi8 (25)));
			str = _mm_add_epi8 (str, _mm_shuffle_epi8 (lut, indices));
		}
		_mm_storeu_si128 ((__m128i *) output, str);
		input += 12;
		output += 16;
		length -= 12;
		consumed += 12;
		quartets -= 4;
	}
	return consumed;
}

static SSSE3_TARGET gsize
decode_block_ssse3 (const guint8 *input, guint8 *output, gboolean uu)
{
	const __m128i lut_hi = _mm_set_epi32 (0x10101010, 0x10101010, 0x08040804, 0x02011010);
	const __m128i lut_lo = _mm_set_epi32 (0x1a1b1b1b, 0x1a131111, 0x11111111, 0x11111115);
	const __m128i lut_shift = _mm_set_epi32 (0, 0, 0xb9b9bfbf, 0x04131000);
	const __m128i pack = _mm_set_epi32 (-1, 0x0c0d0e08, 0x090a0405, 0x06000102);
	guint8 bytes[16];
	__m128i str, hi, lo, invalid, shift, merged;

	str = _mm_loadu_si128 ((const __m128i *) input);
	if (uu) {
		/* Every non-newline byte maps to a UU sextet as (c - 0x20) & 0x3f.
		 * CR and LF need the scalar decoder's line handling. */
		invalid = _mm_or_si128 (_mm_cmpeq_epi8 (str, _mm_set1_epi8 ('\r')),
				       _mm_cmpeq_epi8 (str, _mm_set1_epi8 ('\n')));
		if (_mm_movemask_epi8 (invalid))
			return 0;
		str = _mm_and_si128 (_mm_sub_epi8 (str, _mm_set1_epi8 (32)), _mm_set1_epi8 (63));
	} else {
		/* A Base64 byte is valid iff the AND of its high/low nibble lookups is zero.
		 * Reject the whole block on whitespace, padding or garbage so scalar decoding
		 * can preserve its recovery behavior. */
		hi = _mm_and_si128 (_mm_srli_epi32 (str, 4), _mm_set1_epi8 (0x2f));
		lo = _mm_and_si128 (str, _mm_set1_epi8 (0x2f));
		invalid = _mm_and_si128 (_mm_shuffle_epi8 (lut_hi, hi), _mm_shuffle_epi8 (lut_lo, lo));
		if (_mm_movemask_epi8 (_mm_cmpeq_epi8 (invalid, _mm_setzero_si128 ())) != 0xffff)
			return 0;
		/* The perfect hash (c >> 4) - (c == '/') selects the ASCII-to-sextet offset:
		 * '+' +19, '/' +16, digits +4, uppercase -65, lowercase -71. */
		shift = _mm_shuffle_epi8 (lut_shift, _mm_add_epi8 (hi, _mm_cmpeq_epi8 (str, _mm_set1_epi8 ('/'))));
		str = _mm_add_epi8 (str, shift);
	}
	/* Merge adjacent sextets a,b into (a << 6) | b, then each quartet into
	 * (a << 18) | (b << 12) | (c << 6) | d. Shuffle the three meaningful
	 * bytes of each 32-bit word into big-endian order and pack them together. */
	merged = _mm_maddubs_epi16 (str, _mm_set1_epi32 (0x01400140));
	str = _mm_madd_epi16 (merged, _mm_set1_epi32 (0x00011000));
	str = _mm_shuffle_epi8 (str, pack);
	/* Store only the twelve decoded bytes, never SIMD padding. */
	_mm_storeu_si128 ((__m128i *) bytes, str);
	memcpy (output, bytes, 12);
	return 12;
}

static SSSE3_TARGET gsize
decode_base64_blocks_ssse3 (const guint8 **input, gsize length, guint8 *output)
{
	gsize written = 0;

	while (length >= 16 && decode_block_ssse3 (*input, output + written, FALSE)) {
		*input += 16;
		length -= 16;
		written += 12;
	}

	return written;
}

static AVX2_TARGET gsize
decode_base64_blocks_avx2 (const guint8 **input, gsize length, guint8 *output)
{
	const __m128i lut_hi128 = _mm_set_epi32 (0x10101010, 0x10101010, 0x08040804, 0x02011010);
	const __m128i lut_lo128 = _mm_set_epi32 (0x1a1b1b1b, 0x1a131111, 0x11111111, 0x11111115);
	const __m128i lut_shift128 = _mm_set_epi32 (0, 0, 0xb9b9bfbf, 0x04131000);
	const __m128i pack128 = _mm_set_epi32 (-1, 0x0c0d0e08, 0x090a0405, 0x06000102);
	const __m256i lut_hi = _mm256_broadcastsi128_si256 (lut_hi128);
	const __m256i lut_lo = _mm256_broadcastsi128_si256 (lut_lo128);
	const __m256i lut_shift = _mm256_broadcastsi128_si256 (lut_shift128);
	const __m256i pack = _mm256_broadcastsi128_si256 (pack128);
	const __m256i lanes = _mm256_setr_epi32 (0, 1, 2, 4, 5, 6, -1, -1);
	const __m256i mask = _mm256_set1_epi8 (0x2f);
	const guint8 *inptr = *input;
	gsize written = 0;
	guint consumed;
	__m256i str, hi_nibbles, hi, lo, shift;
	__m128i tail, invalid;

	while (length >= 32) {
		str = _mm256_loadu_si256 ((const __m256i *) inptr);
		hi_nibbles = _mm256_and_si256 (_mm256_srli_epi32 (str, 4), mask);
		hi = _mm256_shuffle_epi8 (lut_hi, hi_nibbles);
		lo = _mm256_shuffle_epi8 (lut_lo, _mm256_and_si256 (str, mask));

		if (!_mm256_testz_si256 (lo, hi)) {
			consumed = inptr[12] == '\n' ? 13 : inptr[12] == '\r' && inptr[13] == '\n' ? 14 : 0;
			invalid = _mm256_castsi256_si128 (_mm256_and_si256 (lo, hi));
			if (!consumed || (_mm_movemask_epi8 (_mm_cmpeq_epi8 (invalid, _mm_setzero_si128 ())) & 0xfff) != 0xfff)
				break;

			/* A 76-character MIME line leaves twelve characters after two vectors.
			 * Decode them without returning to scalar line-break handling. */
			tail = _mm256_castsi256_si128 (str);
			tail = _mm_add_epi8 (tail, _mm_shuffle_epi8 (lut_shift128,
				_mm_add_epi8 (_mm256_castsi256_si128 (hi_nibbles),
					      _mm_cmpeq_epi8 (tail, _mm_set1_epi8 ('/')))));
			tail = _mm_maddubs_epi16 (tail, _mm_set1_epi32 (0x01400140));
			tail = _mm_madd_epi16 (tail, _mm_set1_epi32 (0x00011000));
			tail = _mm_shuffle_epi8 (tail, pack128);

			/* Exact stores preserve output canaries and unconsumed in-place input. */
			_mm_storel_epi64 ((__m128i *) (output + written), tail);
			output[written + 8] = (guint8) _mm_cvtsi128_si32 (_mm_srli_si128 (tail, 8));
			written += 9;
			inptr += consumed;
			length -= consumed;
			continue;
		}

		shift = _mm256_shuffle_epi8 (lut_shift,
			_mm256_add_epi8 (hi_nibbles, _mm256_cmpeq_epi8 (str, mask)));
		str = _mm256_add_epi8 (str, shift);
		str = _mm256_maddubs_epi16 (str, _mm256_set1_epi32 (0x01400140));
		str = _mm256_madd_epi16 (str, _mm256_set1_epi32 (0x00011000));
		str = _mm256_permutevar8x32_epi32 (_mm256_shuffle_epi8 (str, pack), lanes);
		_mm_storeu_si128 ((__m128i *) (output + written), _mm256_castsi256_si128 (str));
		_mm_storel_epi64 ((__m128i *) (output + written + 16), _mm256_extracti128_si256 (str, 1));
		inptr += 32;
		length -= 32;
		written += 24;
	}

	*input = inptr;
	return written + decode_base64_blocks_ssse3 (input, length, output + written);
}

static gboolean
codec_avx2_supported (void)
{
#ifdef _MSC_VER
	int registers[4];

	__cpuid (registers, 1);
	/* Require CPU support and OS preservation of both XMM and YMM state. */
	if ((registers[2] & 0x1c000000) != 0x1c000000 || (_xgetbv (0) & 6) != 6)
		return FALSE;
	__cpuidex (registers, 7, 0);
	return (registers[1] & (1 << 5)) != 0;
#else
	return __builtin_cpu_supports ("avx2");
#endif
}
#endif

static gsize
encode_blocks_scalar (const guint8 *input, gsize length, guint8 *output, guint quartets, gboolean uu)
{
	return 0;
}

static gsize
decode_block_scalar (const guint8 *input, guint8 *output, gboolean uu)
{
	return 0;
}

static gsize
decode_base64_blocks_scalar (const guint8 **input, gsize length, guint8 *output)
{
	return 0;
}

static gsize (*encode_blocks) (const guint8 *, gsize, guint8 *, guint, gboolean) = encode_blocks_scalar;
static gsize (*decode_block) (const guint8 *, guint8 *, gboolean) = decode_block_scalar;
static gsize (*decode_base64_blocks) (const guint8 **, gsize, guint8 *) = decode_base64_blocks_scalar;
static gsize (*scan_base64) (const guint8 *, gsize, GMimeCodecScan *) = scan_base64_scalar;
static gsize (*index_of_qp_special) (const guint8 *, gsize) = index_of_qp_special_scalar;

static void
codec_dispatch_init (void)
{
	static gsize initialized;

	if (g_once_init_enter (&initialized)) {
#ifdef HAVE_SSSE3_CODEC
#ifdef _MSC_VER
		int registers[4];
		__cpuid (registers, 1);
		if (registers[2] & (1 << 9)) {
#else
		if (__builtin_cpu_supports ("ssse3")) {
#endif
			encode_blocks = encode_blocks_ssse3;
			decode_block = decode_block_ssse3;
			decode_base64_blocks = codec_avx2_supported () ? decode_base64_blocks_avx2 : decode_base64_blocks_ssse3;
			scan_base64 = scan_base64_ssse3;
			index_of_qp_special = index_of_qp_special_ssse3;
		}
#endif
		g_once_init_leave (&initialized, 1);
	}
}

gsize
_g_mime_codec_encode_blocks (const guint8 *input, gsize length, guint8 *output, guint quartets, gboolean uu)
{
	codec_dispatch_init ();
	return encode_blocks (input, length, output, quartets, uu);
}

gsize
_g_mime_codec_decode_block (const guint8 *input, guint8 *output, gboolean uu)
{
	codec_dispatch_init ();
	return decode_block (input, output, uu);
}

gsize
_g_mime_codec_decode_base64_blocks (const guint8 **input, gsize length, guint8 *output)
{
	codec_dispatch_init ();
	return decode_base64_blocks (input, length, output);
}

gsize
_g_mime_codec_scan_base64 (const guint8 *input, gsize length, GMimeCodecScan *scan)
{
	memset (scan, 0, sizeof *scan);
	codec_dispatch_init ();
	return scan_base64 (input, length, scan);
}

gsize
_g_mime_codec_index_of_qp_special (const guint8 *input, gsize length)
{
	codec_dispatch_init ();
	return index_of_qp_special (input, length);
}
