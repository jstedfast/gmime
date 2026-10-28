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

/* Content-scanning kernels ported from MimeKit's MimeReader. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-reader-simd-private.h"

#if !defined(GMIME_DISABLE_SIMD) && \
    (((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) || \
     (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))))
#define HAVE_READER_SSE2 1
#define HAVE_READER_AVX2 1
#include <immintrin.h>
#ifdef _MSC_VER
#define SSE2_TARGET
#define AVX2_TARGET
#else
#define SSE2_TARGET __attribute__((target("sse2")))
#define AVX2_TARGET __attribute__((target("avx2,popcnt")))
#endif
#endif

#if !defined(GMIME_DISABLE_SIMD) && \
    (defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON) || defined(__ARM_NEON__))
#define HAVE_READER_NEON 1
#if defined(_MSC_VER) && !defined(__clang__)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif
#endif

#if defined(_MSC_VER) && (defined(HAVE_READER_SSE2) || defined(HAVE_READER_NEON))
#include <intrin.h>
#endif

static gboolean
complete_line (const guint8 *input, gsize length, gsize lf, guint8 first, guint8 second,
	       GMimeReaderScan *scan)
{
	gsize next = lf + 1;

	scan->previous_line = scan->length;
	scan->length = next;
	scan->lines++;
	scan->formats |= lf && input[lf - 1] == '\r' ? 2 : 1;

	/* A one-byte suffix must stay buffered too: its second byte may arrive
	 * on the next read. Unlike the reference, we never read a sentinel. */
	return next < length &&
		((input[next] == '-' && (length - next == 1 || input[next + 1] == '-')) ||
		 (input[next] == first && (length - next == 1 || input[next + 1] == second)));
}

static inline void
scan_tail (const guint8 *input, gsize length, gsize offset, guint8 first, guint8 second,
	   GMimeReaderScan *scan)
{
	gsize i;

	for (i = offset; i < length; i++) {
		if (input[i] == '\n' && complete_line (input, length, i, first, second, scan))
			break;
	}
}

GMimeReaderScan
_g_mime_reader_scan_lines_scalar (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	GMimeReaderScan scan = { 0 };

	scan_tail (input, length, 0, first, second, &scan);

	return scan;
}

#if defined(HAVE_READER_SSE2) || defined(HAVE_READER_NEON)
static inline guint
first_bit (guint32 mask)
{
#ifdef _MSC_VER
	unsigned long bit;

	_BitScanForward (&bit, mask);
	return (guint) bit;
#else
	return (guint) __builtin_ctz (mask);
#endif
}

static inline guint
last_bit (guint32 mask)
{
#ifdef _MSC_VER
	unsigned long bit;

	_BitScanReverse (&bit, mask);
	return (guint) bit;
#else
	return 31 - (guint) __builtin_clz (mask);
#endif
}

static inline guint
mask_popcount (guint32 mask)
{
	mask -= (mask >> 1) & 0x55555555;
	mask = (mask & 0x33333333) + ((mask >> 2) & 0x33333333);
	return (((mask + (mask >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

static inline gboolean
record_lines (GMimeReaderScan *scan, gsize offset, guint32 lf, guint32 crlf, guint32 candidates)
{
	guint32 previous;
	guint last;

	if (candidates) {
		/* Keep the candidate's LF too, without shifting a 32-bit mask by 32. */
		guint32 keep = G_MAXUINT32 >> (31 - first_bit (candidates));

		lf &= keep;
		crlf &= keep;
	}

	last = last_bit (lf);
	previous = lf & ~((guint32) 1 << last);
	scan->previous_line = previous ? offset + last_bit (previous) + 1 : scan->length;
	scan->length = offset + last + 1;
	scan->lines += mask_popcount (lf);

	if (crlf)
		scan->formats |= 2;

	if (lf & ~crlf)
		scan->formats |= 1;

	return candidates != 0;
}
#endif

#ifdef HAVE_READER_SSE2
static SSE2_TARGET GMimeReaderScan
scan_lines_sse2 (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	const __m128i newline = _mm_set1_epi8 ('\n');
	const __m128i carriage_return = _mm_set1_epi8 ('\r');
	const __m128i dash = _mm_set1_epi8 ('-');
	const __m128i c1 = _mm_set1_epi8 ((char) first);
	const __m128i c2 = _mm_set1_epi8 ((char) second);
	GMimeReaderScan scan = { 0 };
	gsize i = 0;

	/* Unlike MimeReader, there is no sentinel: both shifted loads must fit. */
	while (length - i >= 18) {
		__m128i block = _mm_loadu_si128 ((const __m128i *) (input + i));
		guint32 lf = (guint32) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, newline));
		guint32 remaining = lf & (lf - 1);

		/* Sparse masks need no population count or vector prefix loads. */
		if (lf && !(remaining & (remaining - 1))) {
			if (complete_line (input, length, i + first_bit (lf), first, second, &scan))
				return scan;

			if (remaining && complete_line (input, length, i + first_bit (remaining), first, second, &scan))
				return scan;

			i += 16;
			continue;
		}

		if (lf) {
			guint32 cr = (guint32) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, carriage_return));
			/* Recover the carry only when needed, keeping LF-free blocks cheap. */
			guint32 carry = i && input[i - 1] == '\r';
			__m128i n1 = _mm_loadu_si128 ((const __m128i *) (input + i + 1));
			__m128i n2 = _mm_loadu_si128 ((const __m128i *) (input + i + 2));
			__m128i possible = _mm_or_si128 (
				_mm_and_si128 (_mm_cmpeq_epi8 (n1, dash), _mm_cmpeq_epi8 (n2, dash)),
				_mm_and_si128 (_mm_cmpeq_epi8 (n1, c1), _mm_cmpeq_epi8 (n2, c2)));
			guint32 candidates = lf & (guint32) _mm_movemask_epi8 (possible);
			guint32 crlf = lf & ((cr << 1) | carry);

			if (record_lines (&scan, i, lf, crlf, candidates))
				return scan;
		}

		i += 16;
	}

	scan_tail (input, length, i, first, second, &scan);

	return scan;
}

static AVX2_TARGET GMimeReaderScan
scan_lines_avx2 (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	const __m256i newline = _mm256_set1_epi8 ('\n');
	const __m256i carriage_return = _mm256_set1_epi8 ('\r');
	const __m256i dash = _mm256_set1_epi8 ('-');
	const __m256i c1 = _mm256_set1_epi8 ((char) first);
	const __m256i c2 = _mm256_set1_epi8 ((char) second);
	GMimeReaderScan scan = { 0 };
	gsize i = 0;

	while (length - i >= 34) {
		__m256i block = _mm256_loadu_si256 ((const __m256i *) (input + i));
		guint32 lf = (guint32) _mm256_movemask_epi8 (_mm256_cmpeq_epi8 (block, newline));

		if (lf) {
			guint32 cr = (guint32) _mm256_movemask_epi8 (_mm256_cmpeq_epi8 (block, carriage_return));
			guint32 carry = i && input[i - 1] == '\r';
			__m256i n1 = _mm256_loadu_si256 ((const __m256i *) (input + i + 1));
			__m256i n2 = _mm256_loadu_si256 ((const __m256i *) (input + i + 2));
			__m256i possible = _mm256_or_si256 (
				_mm256_and_si256 (_mm256_cmpeq_epi8 (n1, dash), _mm256_cmpeq_epi8 (n2, dash)),
				_mm256_and_si256 (_mm256_cmpeq_epi8 (n1, c1), _mm256_cmpeq_epi8 (n2, c2)));
			guint32 candidates = lf & (guint32) _mm256_movemask_epi8 (possible);
			guint32 crlf = lf & ((cr << 1) | carry);

			if (record_lines (&scan, i, lf, crlf, candidates))
				return scan;
		}

		i += 32;
	}

	_mm256_zeroupper ();
	scan_tail (input, length, i, first, second, &scan);

	return scan;
}

static gboolean
have_sse2 (void)
{
#ifdef _MSC_VER
	int info[4];

	__cpuid (info, 1);
	return (info[3] & (1 << 26)) != 0;
#else
	__builtin_cpu_init ();
	return __builtin_cpu_supports ("sse2");
#endif
}

static gboolean
have_avx2 (void)
{
#ifdef _MSC_VER
	int info[4];

	__cpuid (info, 0);

	if (info[0] < 7)
		return FALSE;

	__cpuid (info, 1);

	/* AVX needs both CPU support and OS preservation of XMM/YMM state. */
	if ((info[2] & 0x18800000) != 0x18800000 || (_xgetbv (0) & 6) != 6)
		return FALSE;

	__cpuidex (info, 7, 0);

	return (info[1] & (1 << 5)) != 0;
#else
	__builtin_cpu_init ();
	return __builtin_cpu_supports ("avx2") && __builtin_cpu_supports ("popcnt");
#endif
}
#endif

#ifdef HAVE_READER_NEON
static inline guint32
neon_mask (uint8x16_t value)
{
	static const guint8 weights[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	uint16x8_t pairs = vpaddlq_u8 (vandq_u8 (value, vld1q_u8 (weights)));
	uint32x4_t quads = vpaddlq_u16 (pairs);
	uint64x2_t halves = vpaddlq_u32 (quads);

	return (guint32) vgetq_lane_u64 (halves, 0) | ((guint32) vgetq_lane_u64 (halves, 1) << 8);
}

static GMimeReaderScan
scan_lines_neon (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	const uint8x16_t newline = vdupq_n_u8 ('\n');
	const uint8x16_t carriage_return = vdupq_n_u8 ('\r');
	const uint8x16_t dash = vdupq_n_u8 ('-');
	const uint8x16_t c1 = vdupq_n_u8 (first);
	const uint8x16_t c2 = vdupq_n_u8 (second);
	GMimeReaderScan scan = { 0 };
	gsize i = 0;

	while (length - i >= 18) {
		uint8x16_t block = vld1q_u8 (input + i);
		guint32 lf = neon_mask (vceqq_u8 (block, newline));

		if (lf) {
			guint32 cr = neon_mask (vceqq_u8 (block, carriage_return));
			guint32 carry = i && input[i - 1] == '\r';
			uint8x16_t n1 = vld1q_u8 (input + i + 1);
			uint8x16_t n2 = vld1q_u8 (input + i + 2);
			uint8x16_t possible = vorrq_u8 (
				vandq_u8 (vceqq_u8 (n1, dash), vceqq_u8 (n2, dash)),
				vandq_u8 (vceqq_u8 (n1, c1), vceqq_u8 (n2, c2)));
			guint32 candidates = lf & neon_mask (possible);
			guint32 crlf = lf & ((cr << 1) | carry);

			if (record_lines (&scan, i, lf, crlf, candidates))
				return scan;
		}

		i += 16;
	}

	scan_tail (input, length, i, first, second, &scan);

	return scan;
}
#endif

GMimeReaderScanFunc
_g_mime_reader_scan_lines_get_backend (GMimeReaderScanBackend backend)
{
	switch (backend) {
	case GMIME_READER_SCAN_SCALAR:
		return _g_mime_reader_scan_lines_scalar;
	case GMIME_READER_SCAN_SSE2:
#ifdef HAVE_READER_SSE2
		if (have_sse2 ())
			return scan_lines_sse2;
#endif
		return NULL;
	case GMIME_READER_SCAN_AVX2:
#ifdef HAVE_READER_AVX2
		if (have_avx2 ())
			return scan_lines_avx2;
#endif
		return NULL;
	case GMIME_READER_SCAN_NEON:
#ifdef HAVE_READER_NEON
		return scan_lines_neon;
#else
		return NULL;
#endif
	default:
		g_return_val_if_reached (NULL);
	}
}

GMimeReaderScan
_g_mime_reader_scan_lines (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	static GMimeReaderScanFunc scanner;
	static gsize initialized;

	if (g_once_init_enter (&initialized)) {
		scanner = _g_mime_reader_scan_lines_get_backend (GMIME_READER_SCAN_AVX2);
		if (!scanner)
			scanner = _g_mime_reader_scan_lines_get_backend (GMIME_READER_SCAN_SSE2);
		if (!scanner)
			scanner = _g_mime_reader_scan_lines_get_backend (GMIME_READER_SCAN_NEON);
		if (!scanner)
			scanner = _g_mime_reader_scan_lines_scalar;
		g_once_init_leave (&initialized, 1);
	}

	return scanner (input, length, first, second);
}
