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
#include <string.h>

#if !defined(GMIME_DISABLE_SIMD) && \
    (((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) || \
     (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))))
#define HAVE_READER_SSE2 1
#define HAVE_READER_AVX2 1
#include <immintrin.h>
#ifdef _MSC_VER
#define SSE2_TARGET
#define SSE41_TARGET
#define AVX2_TARGET
#else
#define SSE2_TARGET __attribute__((target("sse2")))
#define SSE41_TARGET __attribute__((target("ssse3,sse4.1")))
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

static gboolean
is_base64_alphabet (guint8 c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '+' || c == '/';
}

static GMimeReaderContentScan
scan_content_scalar (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options)
{
	GMimeReaderContentScan result = { 0 };
	guint32 alphabet = 0;
	guint64 line_length = options->line_length;
	gsize i = 0;

	while (i < length) {
		guint8 c = input[i];

		if ((options->detect_null && c == 0) || (options->detect_eight_bit && c >= 128))
			break;

		if (options->encoding == GMIME_CONTENT_ENCODING_BASE64) {
			if (is_base64_alphabet (c))
				alphabet++;
			else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
				break;
		} else if (options->encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE && c == '=') {
			if (length - i < 3 ||
			    !((g_ascii_isxdigit (input[i + 1]) && g_ascii_isxdigit (input[i + 2])) ||
			      (input[i + 1] == '\r' && input[i + 2] == '\n')))
				break;
		}

		if (line_length + i + 1 - result.scan.length > 1000)
			break;

		if (c == '\n') {
			gboolean boundary;

			if (!(i ? input[i - 1] == '\r' : options->midline_cr))
				break;

			result.alphabet_count = alphabet;
			line_length = 0;

			boundary = complete_line (input, length, i++, options->first, options->second, &result.scan);
			result.scan.formats = 2;

			if (boundary)
				break;
		} else {
			i++;
		}
	}

	result.examined = MAX (i, result.scan.length);

	return result;
}

#if defined(HAVE_READER_SSE2) || defined(HAVE_READER_NEON)
static inline gboolean
qp_escapes_valid (const guint8 *input, gsize length, guint32 equals, guint32 hex,
		  guint32 cr, guint32 lf, guint width)
{
	guint32 tail = equals & ((guint32) 3 << (width - 2));
	guint32 valid = ((hex >> 1) & (hex >> 2)) | ((cr >> 1) & (lf >> 2));

	if (equals & ~tail & ~valid)
		return FALSE;

	while (tail) {
		guint i = first_bit (tail);

		if (length - i < 3 ||
		    !((g_ascii_isxdigit (input[i + 1]) && g_ascii_isxdigit (input[i + 2])) ||
		      (input[i + 1] == '\r' && input[i + 2] == '\n')))
			return FALSE;

		tail &= tail - 1;
	}

	return TRUE;
}

/* Each instantiation has a constant encoding: no encoding dispatch or
 * indirect classification calls occur inside the vector loop. */
#define DEFINE_CONTENT_SCAN(name, target, width, vector, load, prefix, mode) \
static target GMimeReaderContentScan \
name (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options) \
{ \
	GMimeReaderContentScan result = { 0 }; \
	guint64 line_length = options->line_length; \
	guint32 carry = options->midline_cr, alphabet_total = 0; \
	const guint32 all = G_MAXUINT32 >> (32 - width); \
	gsize i = 0; \
 \
	while (length - i >= width + 2) { \
		vector block = load (input + i); \
		guint32 alphabet = mode == GMIME_CONTENT_ENCODING_BASE64 ? prefix##_alphabet (block) : 0; \
		guint32 lf, cr, candidates = 0; \
 \
		if (mode != GMIME_CONTENT_ENCODING_BASE64 && \
		    ((options->detect_null && prefix##_equal (block, 0)) || \
		     (options->detect_eight_bit && prefix##_high (block)))) \
			break; \
 \
		if (mode == GMIME_CONTENT_ENCODING_BASE64 && alphabet == all) { \
			if (line_length + i + width - result.scan.length > 1000) \
				break; \
 \
			alphabet_total += width; \
			i += width; \
			carry = 0; \
			continue; \
		} \
 \
		lf = prefix##_equal (block, '\n'); \
		cr = prefix##_equal (block, '\r'); \
 \
		if (mode == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE) { \
			guint32 equals = prefix##_equal (block, '='); \
 \
			if (equals && !qp_escapes_valid (input + i, length - i, equals, prefix##_hex (block), cr, lf, width)) \
				break; \
		} \
 \
		if (mode == GMIME_CONTENT_ENCODING_BASE64 && \
		    (alphabet | lf | cr | prefix##_equal (block, ' ') | prefix##_equal (block, '\t')) != all) \
			break; \
 \
		if (lf) { \
			if ((lf & ~((cr << 1) | carry)) || \
			    line_length + i + first_bit (lf) + 1 - result.scan.length > 1000) \
				break; \
 \
			if (mode != GMIME_CONTENT_ENCODING_BASE64 || options->first != '-') { \
				vector n1 = load (input + i + 1), n2 = load (input + i + 2); \
				candidates = lf & ((prefix##_equal (n1, '-') & prefix##_equal (n2, '-')) | \
					(prefix##_equal (n1, options->first) & prefix##_equal (n2, options->second))); \
			} \
 \
			if (candidates) \
				lf &= G_MAXUINT32 >> (31 - first_bit (candidates)); \
 \
			if (mode == GMIME_CONTENT_ENCODING_BASE64) \
				result.alphabet_count = alphabet_total + mask_popcount (alphabet & (G_MAXUINT32 >> (31 - last_bit (lf)))); \
 \
			record_lines (&result.scan, i, lf, lf, 0); \
			line_length = 0; \
 \
			if (candidates) \
				break; \
		} else if (line_length + i + width - result.scan.length > 1000) { \
			break; \
		} \
 \
		carry = cr >> (width - 1); \
		alphabet_total += mask_popcount (alphabet); \
		i += width; \
	} \
 \
	result.examined = MAX (i, result.scan.length); \
 \
	return result; \
}
#endif

#ifdef HAVE_READER_SSE2
static inline SSE2_TARGET __m128i
content_sse2_load (const guint8 *input)
{
	return _mm_loadu_si128 ((const __m128i *) input);
}

static inline SSE2_TARGET guint32
content_sse2_equal (__m128i block, guint8 c)
{
	return (guint32) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, _mm_set1_epi8 ((char) c)));
}

static inline SSE2_TARGET guint32
content_sse2_high (__m128i block)
{
	return (guint32) _mm_movemask_epi8 (block);
}

static inline SSE2_TARGET guint32
content_sse2_range (__m128i block, guint8 first, guint8 count)
{
	return (guint32) _mm_movemask_epi8 (_mm_cmplt_epi8 (
		_mm_add_epi8 (block, _mm_set1_epi8 ((char) (128 - first))),
		_mm_set1_epi8 ((char) (-128 + count))));
}

static inline SSE2_TARGET guint32
content_sse2_alphabet (__m128i block)
{
	return content_sse2_range (_mm_or_si128 (block, _mm_set1_epi8 (32)), 'a', 26) |
		content_sse2_range (block, '0', 10) | content_sse2_equal (block, '+') | content_sse2_equal (block, '/');
}

static inline SSE2_TARGET guint32
content_sse2_hex (__m128i block)
{
	return content_sse2_range (_mm_or_si128 (block, _mm_set1_epi8 (32)), 'a', 6) |
		content_sse2_range (block, '0', 10);
}

static inline AVX2_TARGET __m256i
content_avx2_load (const guint8 *input)
{
	return _mm256_loadu_si256 ((const __m256i *) input);
}

static inline AVX2_TARGET guint32
content_avx2_equal (__m256i block, guint8 c)
{
	return (guint32) _mm256_movemask_epi8 (_mm256_cmpeq_epi8 (block, _mm256_set1_epi8 ((char) c)));
}

static inline AVX2_TARGET guint32
content_avx2_high (__m256i block)
{
	return (guint32) _mm256_movemask_epi8 (block);
}

static inline AVX2_TARGET guint32
content_avx2_range (__m256i block, guint8 first, guint8 count)
{
	return (guint32) _mm256_movemask_epi8 (_mm256_cmpgt_epi8 (_mm256_set1_epi8 ((char) (-128 + count)),
		_mm256_add_epi8 (block, _mm256_set1_epi8 ((char) (128 - first)))));
}

static inline AVX2_TARGET guint32
content_avx2_alphabet (__m256i block)
{
	return content_avx2_range (_mm256_or_si256 (block, _mm256_set1_epi8 (32)), 'a', 26) |
		content_avx2_range (block, '0', 10) | content_avx2_equal (block, '+') | content_avx2_equal (block, '/');
}

static inline AVX2_TARGET guint32
content_avx2_hex (__m256i block)
{
	return content_avx2_range (_mm256_or_si256 (block, _mm256_set1_epi8 (32)), 'a', 6) |
		content_avx2_range (block, '0', 10);
}

DEFINE_CONTENT_SCAN (scan_clean_sse2, SSE2_TARGET, 16, __m128i, content_sse2_load, content_sse2, GMIME_CONTENT_ENCODING_UNSPECIFIED)
DEFINE_CONTENT_SCAN (scan_base64_sse2, SSE2_TARGET, 16, __m128i, content_sse2_load, content_sse2, GMIME_CONTENT_ENCODING_BASE64)
DEFINE_CONTENT_SCAN (scan_qp_sse2, SSE2_TARGET, 16, __m128i, content_sse2_load, content_sse2, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE)
DEFINE_CONTENT_SCAN (scan_clean_avx2, AVX2_TARGET, 32, __m256i, content_avx2_load, content_avx2, GMIME_CONTENT_ENCODING_UNSPECIFIED)
DEFINE_CONTENT_SCAN (scan_base64_avx2, AVX2_TARGET, 32, __m256i, content_avx2_load, content_avx2, GMIME_CONTENT_ENCODING_BASE64)
DEFINE_CONTENT_SCAN (scan_qp_avx2, AVX2_TARGET, 32, __m256i, content_avx2_load, content_avx2, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE)
#endif

#ifdef HAVE_READER_NEON
static inline guint32
content_neon_equal (uint8x16_t block, guint8 c)
{
	return neon_mask (vceqq_u8 (block, vdupq_n_u8 (c)));
}

static inline guint32
content_neon_high (uint8x16_t block)
{
	return neon_mask (vcgeq_u8 (block, vdupq_n_u8 (128)));
}

static inline guint32
content_neon_range (uint8x16_t block, guint8 first, guint8 count)
{
	return neon_mask (vcltq_u8 (vsubq_u8 (block, vdupq_n_u8 (first)), vdupq_n_u8 (count)));
}

static inline guint32
content_neon_alphabet (uint8x16_t block)
{
	return content_neon_range (vorrq_u8 (block, vdupq_n_u8 (32)), 'a', 26) |
		content_neon_range (block, '0', 10) | content_neon_equal (block, '+') | content_neon_equal (block, '/');
}

static inline guint32
content_neon_hex (uint8x16_t block)
{
	return content_neon_range (vorrq_u8 (block, vdupq_n_u8 (32)), 'a', 6) |
		content_neon_range (block, '0', 10);
}

DEFINE_CONTENT_SCAN (scan_clean_neon, , 16, uint8x16_t, vld1q_u8, content_neon, GMIME_CONTENT_ENCODING_UNSPECIFIED)
DEFINE_CONTENT_SCAN (scan_base64_neon, , 16, uint8x16_t, vld1q_u8, content_neon, GMIME_CONTENT_ENCODING_BASE64)
DEFINE_CONTENT_SCAN (scan_qp_neon, , 16, uint8x16_t, vld1q_u8, content_neon, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE)
#endif

#undef DEFINE_CONTENT_SCAN

static gboolean
uu_payload_scalar (const guint8 *input, gsize length)
{
	gsize i;

	for (i = 0; i < length; i++) {
		if (input[i] < 33 || input[i] > 96)
			return FALSE;
	}

	return TRUE;
}

typedef gboolean (*UUContentFunc) (const guint8 *input, gsize length);

#ifdef HAVE_READER_SSE2
static SSE2_TARGET gboolean
uu_payload_sse2 (const guint8 *input, gsize length)
{
	while (length >= 16) {
		if (content_sse2_range (content_sse2_load (input), 33, 64) != 0xffff)
			return FALSE;

		input += 16;
		length -= 16;
	}

	return uu_payload_scalar (input, length);
}

static AVX2_TARGET gboolean
uu_payload_avx2 (const guint8 *input, gsize length)
{
	if (length >= 32)
		return content_avx2_range (content_avx2_load (input), 33, 64) == G_MAXUINT32 &&
			content_avx2_range (content_avx2_load (input + length - 32), 33, 64) == G_MAXUINT32;

	return uu_payload_sse2 (input, length);
}
#endif

#ifdef HAVE_READER_NEON
static gboolean
uu_payload_neon (const guint8 *input, gsize length)
{
	while (length >= 16) {
		if (content_neon_range (vld1q_u8 (input), 33, 64) != 0xffff)
			return FALSE;

		input += 16;
		length -= 16;
	}

	return uu_payload_scalar (input, length);
}
#endif

static GMimeReaderContentScan
scan_uu_lines (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options,
	       UUContentFunc valid)
{
	GMimeReaderContentScan result = { 0 };
	gint remaining = options->remaining;
	guint64 line_length = options->line_length;
	gboolean midline_cr = options->midline_cr;
	gsize i = 0;

	while (i < length) {
		gsize payload = i, count, end, newline = remaining == 0 && midline_cr ? 1 : 2;

		if (remaining < 0) {
			gint decoded = input[i] - 32;

			if (input[i] == '-' || input[i] == options->first || decoded < 1 || decoded > 45)
				break;
			count = 4 * ((decoded + 2) / 3);
			payload++;
		} else {
			count = remaining;
		}

		if (count > length - payload || length - payload - count < newline)
			break;

		end = payload + count;

		if (input[end + newline - 1] != '\n' || (newline == 2 && input[end] != '\r') ||
		    line_length + end + newline - i > 1000 || !valid (input + payload, count))
			break;

		result.scan.previous_line = result.scan.length;
		result.scan.length = end + newline;
		result.scan.lines++;
		result.scan.formats = 2;
		i = result.scan.length;
		remaining = -1;
		midline_cr = FALSE;
		line_length = 0;
	}

	result.examined = result.scan.length;

	return result;
}

static GMimeReaderContentScan
scan_content_scalar_dispatch (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options)
{
	if (options->encoding == GMIME_CONTENT_ENCODING_UUENCODE)
		return scan_uu_lines (input, length, options, uu_payload_scalar);

	return scan_content_scalar (input, length, options);
}

#define DEFINE_CONTENT_DISPATCH(backend) \
static GMimeReaderContentScan \
scan_content_##backend (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options) \
{ \
	switch (options->encoding) { \
	case GMIME_CONTENT_ENCODING_BASE64: return scan_base64_##backend (input, length, options); \
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE: return scan_qp_##backend (input, length, options); \
	case GMIME_CONTENT_ENCODING_UUENCODE: return scan_uu_lines (input, length, options, uu_payload_##backend); \
	default: return scan_clean_##backend (input, length, options); \
	} \
}

#ifdef HAVE_READER_SSE2
DEFINE_CONTENT_DISPATCH (sse2)
DEFINE_CONTENT_DISPATCH (avx2)
#endif
#ifdef HAVE_READER_NEON
DEFINE_CONTENT_DISPATCH (neon)
#endif
#undef DEFINE_CONTENT_DISPATCH

GMimeReaderContentScanFunc
_g_mime_reader_scan_content_get_backend (GMimeReaderScanBackend backend)
{
	switch (backend) {
	case GMIME_READER_SCAN_SCALAR:
		return scan_content_scalar_dispatch;
	case GMIME_READER_SCAN_SSE2:
#ifdef HAVE_READER_SSE2
		if (have_sse2 ())
			return scan_content_sse2;
#endif
		return NULL;
	case GMIME_READER_SCAN_AVX2:
#ifdef HAVE_READER_AVX2
		if (have_avx2 ())
			return scan_content_avx2;
#endif
		return NULL;
	case GMIME_READER_SCAN_NEON:
#ifdef HAVE_READER_NEON
		return scan_content_neon;
#else
		return NULL;
#endif
	default:
		g_return_val_if_reached (NULL);
	}
}

GMimeReaderContentScan
_g_mime_reader_scan_content (const guint8 *input, gsize length, const GMimeReaderContentScanOptions *options)
{
	static GMimeReaderContentScanFunc scanner;
	static gsize initialized;

	if (g_once_init_enter (&initialized)) {
		scanner = _g_mime_reader_scan_content_get_backend (GMIME_READER_SCAN_AVX2);
		if (!scanner)
			scanner = _g_mime_reader_scan_content_get_backend (GMIME_READER_SCAN_SSE2);
		if (!scanner)
			scanner = _g_mime_reader_scan_content_get_backend (GMIME_READER_SCAN_NEON);
		if (!scanner)
			scanner = scan_content_scalar_dispatch;
		g_once_init_leave (&initialized, 1);
	}

	return scanner (input, length, options);
}

static void
scan_line_tail (const guint8 *input, gsize length, gsize offset, gboolean detect_null,
		gboolean detect_eight_bit, GMimeReaderLineScan *scan)
{
	const gsize ones = G_MAXSIZE / 255, high_bits = ones * 128;
	gsize i;

	while (length - offset >= sizeof (gsize)) {
		gsize word, lf, nulls;

		memcpy (&word, input + offset, sizeof (word));
		lf = word ^ (ones * 10);

		if ((lf - ones) & ~lf & high_bits)
			break;

		nulls = (word - ones) & ~word & high_bits;

		if (detect_null && scan->null_index == G_MAXSIZE && nulls) {
			for (i = 0; input[offset + i] != 0; i++)
				;
			scan->null_index = offset + i;
		}

		if (detect_eight_bit && scan->eight_bit_index == G_MAXSIZE && (word & high_bits)) {
			for (i = 0; input[offset + i] < 128; i++)
				;
			scan->eight_bit_index = offset + i;
		}

		offset += sizeof (gsize);
	}

	for (; offset < length; offset++) {
		guint8 c = input[offset];

		if (c == '\n') {
			scan->length = offset;
			break;
		}

		if (detect_null && c == 0 && scan->null_index == G_MAXSIZE)
			scan->null_index = offset;

		if (detect_eight_bit && c >= 128 && scan->eight_bit_index == G_MAXSIZE)
			scan->eight_bit_index = offset;
	}
}

static GMimeReaderLineScan
scan_line_scalar (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	GMimeReaderLineScan scan = { length, G_MAXSIZE, G_MAXSIZE };

	scan_line_tail (input, length, 0, detect_null, detect_eight_bit, &scan);

	return scan;
}

#if defined(HAVE_READER_SSE2) || defined(HAVE_READER_NEON)
static inline gboolean
record_special_bytes (GMimeReaderLineScan *scan, gsize offset, guint32 lf, guint32 nulls, guint32 eight_bits)
{
	if (lf) {
		guint32 before = lf ^ (lf - 1);

		nulls &= before;
		eight_bits &= before;
		scan->length = offset + first_bit (lf);
	}

	if (nulls && scan->null_index == G_MAXSIZE)
		scan->null_index = offset + first_bit (nulls);

	if (eight_bits && scan->eight_bit_index == G_MAXSIZE)
		scan->eight_bit_index = offset + first_bit (eight_bits);

	return lf != 0;
}
#endif

#ifdef HAVE_READER_SSE2
static SSE2_TARGET GMimeReaderLineScan
scan_line_sse2 (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	const __m128i newline = _mm_set1_epi8 ('\n'), zero = _mm_setzero_si128 ();
	GMimeReaderLineScan scan = { length, G_MAXSIZE, G_MAXSIZE };
	gsize i = 0;

	while (length - i >= 16) {
		__m128i block = _mm_loadu_si128 ((const __m128i *) (input + i));
		guint32 lf = (guint32) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, newline));
		guint32 nulls = detect_null ? (guint32) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, zero)) : 0;
		guint32 eight_bits = detect_eight_bit ? (guint32) _mm_movemask_epi8 (block) : 0;

		if (record_special_bytes (&scan, i, lf, nulls, eight_bits))
			return scan;

		detect_null = detect_null && scan.null_index == G_MAXSIZE;
		detect_eight_bit = detect_eight_bit && scan.eight_bit_index == G_MAXSIZE;
		i += 16;
	}

	scan_line_tail (input, length, i, detect_null, detect_eight_bit, &scan);

	return scan;
}

static AVX2_TARGET GMimeReaderLineScan
scan_line_avx2 (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	const __m256i newline = _mm256_set1_epi8 ('\n'), zero = _mm256_setzero_si256 ();
	GMimeReaderLineScan scan = { length, G_MAXSIZE, G_MAXSIZE };
	gsize i = 0;

	while (length - i >= 32) {
		__m256i block = _mm256_loadu_si256 ((const __m256i *) (input + i));
		guint32 lf = (guint32) _mm256_movemask_epi8 (_mm256_cmpeq_epi8 (block, newline));
		guint32 nulls = detect_null ? (guint32) _mm256_movemask_epi8 (_mm256_cmpeq_epi8 (block, zero)) : 0;
		guint32 eight_bits = detect_eight_bit ? (guint32) _mm256_movemask_epi8 (block) : 0;

		if (record_special_bytes (&scan, i, lf, nulls, eight_bits))
			return scan;

		detect_null = detect_null && scan.null_index == G_MAXSIZE;
		detect_eight_bit = detect_eight_bit && scan.eight_bit_index == G_MAXSIZE;
		i += 32;
	}

	_mm256_zeroupper ();
	scan_line_tail (input, length, i, detect_null, detect_eight_bit, &scan);

	return scan;
}
#endif

#ifdef HAVE_READER_NEON
static GMimeReaderLineScan
scan_line_neon (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	GMimeReaderLineScan scan = { length, G_MAXSIZE, G_MAXSIZE };
	gsize i = 0;

	while (length - i >= 16) {
		uint8x16_t block = vld1q_u8 (input + i);
		guint32 lf = neon_mask (vceqq_u8 (block, vdupq_n_u8 ('\n')));
		guint32 nulls = detect_null ? neon_mask (vceqq_u8 (block, vdupq_n_u8 (0))) : 0;
		guint32 eight_bits = detect_eight_bit ? neon_mask (vcgeq_u8 (block, vdupq_n_u8 (128))) : 0;

		if (record_special_bytes (&scan, i, lf, nulls, eight_bits))
			return scan;

		detect_null = detect_null && scan.null_index == G_MAXSIZE;
		detect_eight_bit = detect_eight_bit && scan.eight_bit_index == G_MAXSIZE;
		i += 16;
	}

	scan_line_tail (input, length, i, detect_null, detect_eight_bit, &scan);

	return scan;
}
#endif

GMimeReaderLineScanFunc
_g_mime_reader_scan_line_get_backend (GMimeReaderScanBackend backend)
{
	switch (backend) {
	case GMIME_READER_SCAN_SCALAR:
		return scan_line_scalar;
	case GMIME_READER_SCAN_SSE2:
#ifdef HAVE_READER_SSE2
		if (have_sse2 ())
			return scan_line_sse2;
#endif
		return NULL;
	case GMIME_READER_SCAN_AVX2:
#ifdef HAVE_READER_AVX2
		if (have_avx2 ())
			return scan_line_avx2;
#endif
		return NULL;
	case GMIME_READER_SCAN_NEON:
#ifdef HAVE_READER_NEON
		return scan_line_neon;
#else
		return NULL;
#endif
	default:
		g_return_val_if_reached (NULL);
	}
}

GMimeReaderLineScan
_g_mime_reader_scan_line (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	static GMimeReaderLineScanFunc scanner;
	static gsize initialized;

	if (g_once_init_enter (&initialized)) {
		scanner = _g_mime_reader_scan_line_get_backend (GMIME_READER_SCAN_AVX2);
		if (!scanner)
			scanner = _g_mime_reader_scan_line_get_backend (GMIME_READER_SCAN_SSE2);
		if (!scanner)
			scanner = _g_mime_reader_scan_line_get_backend (GMIME_READER_SCAN_NEON);
		if (!scanner)
			scanner = scan_line_scalar;
		g_once_init_leave (&initialized, 1);
	}

	return scanner (input, length, detect_null, detect_eight_bit);
}

static gsize
utf8_invalid_index_scalar (const guint8 *input, gsize length)
{
	gsize i = 0;

	while (i < length) {
		guint8 c = input[i], lower = 0x80, upper = 0xbf;
		guint count, j;

		if (c < 0x80) {
			i++;
			continue;
		}

		if (c < 0xc2 || c > 0xf4)
			return i;

		if (c < 0xe0) {
			count = 2;
		} else if (c < 0xf0) {
			count = 3;
			if (c == 0xe0)
				lower = 0xa0;
			else if (c == 0xed)
				upper = 0x9f;
		} else {
			count = 4;
			if (c == 0xf0)
				lower = 0x90;
			else if (c == 0xf4)
				upper = 0x8f;
		}

		if (length - i < count || input[i + 1] < lower || input[i + 1] > upper)
			return i;

		for (j = 2; j < count; j++) {
			if ((input[i + j] & 0xc0) != 0x80)
				return i;
		}

		i += count;
	}

	return length;
}

#if defined(HAVE_READER_SSE2) || (defined(HAVE_READER_NEON) && (defined(__aarch64__) || defined(_M_ARM64)))
/*
 * UTF-8 lookup algorithm adapted via MimeKit from simdutf/SimdUnicode,
 * revision d20b63710e8893dede79890f29fd5ae6745d3e2d.
 * Copyright (c) 2023 SimdUnicode authors.
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
static const guint8 utf8_high[16] = { 2, 2, 2, 2, 2, 2, 2, 2, 128, 128, 128, 128, 33, 1, 21, 73 };
static const guint8 utf8_low[16] = {
	0xe7, 0xa3, 0x83, 0x83, 0x8b, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xdb, 0xcb, 0xcb
};
static const guint8 utf8_current[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 0xe6, 0xae, 0xba, 0xba, 1, 1, 1, 1 };
static const guint8 utf8_max[32] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0xef, 0xdf, 0xbf
};

static gsize
utf8_invalid_index_tail (const guint8 *input, gsize length, gsize offset, gboolean incomplete)
{
	if (offset == length && !incomplete)
		return length;

	if (incomplete) {
		offset -= 3;
		while (offset && (input[offset] & 0xc0) == 0x80)
			offset--;
	}

	return offset + utf8_invalid_index_scalar (input + offset, length - offset);
}
#endif

#ifdef HAVE_READER_SSE2
static SSE41_TARGET gsize
utf8_invalid_index_sse41 (const guint8 *input, gsize length)
{
	const __m128i high = _mm_loadu_si128 ((const __m128i *) utf8_high);
	const __m128i low = _mm_loadu_si128 ((const __m128i *) utf8_low);
	const __m128i next = _mm_loadu_si128 ((const __m128i *) utf8_current);
	const __m128i max = _mm_loadu_si128 ((const __m128i *) (utf8_max + 16));
	const __m128i nibble = _mm_set1_epi8 (15), high_bit = _mm_set1_epi8 ((char) 0x80);
	const __m128i third = _mm_set1_epi8 (0x60), fourth = _mm_set1_epi8 (0x70);
	__m128i previous = _mm_setzero_si128 (), incomplete = _mm_setzero_si128 ();
	gsize offset = 0;

	while (length - offset >= 16) {
		__m128i current = _mm_loadu_si128 ((const __m128i *) (input + offset));

		if (!_mm_movemask_epi8 (current)) {
			if (!_mm_testz_si128 (incomplete, incomplete))
				break;

			incomplete = _mm_setzero_si128 ();
		} else {
			__m128i p1 = _mm_alignr_epi8 (current, previous, 15);
			__m128i p2 = _mm_alignr_epi8 (current, previous, 14);
			__m128i p3 = _mm_alignr_epi8 (current, previous, 13);
			__m128i h = _mm_shuffle_epi8 (high, _mm_and_si128 (_mm_srli_epi16 (p1, 4), nibble));
			__m128i l = _mm_shuffle_epi8 (low, _mm_and_si128 (p1, nibble));
			__m128i n = _mm_shuffle_epi8 (next, _mm_and_si128 (_mm_srli_epi16 (current, 4), nibble));
			__m128i required = _mm_and_si128 (high_bit,
				_mm_or_si128 (_mm_subs_epu8 (p2, third), _mm_subs_epu8 (p3, fourth)));
			__m128i errors = _mm_xor_si128 (_mm_and_si128 (_mm_and_si128 (h, l), n), required);

			if (!_mm_testz_si128 (errors, errors))
				break;

			incomplete = _mm_subs_epu8 (current, max);
		}

		previous = current;
		offset += 16;
	}

	return utf8_invalid_index_tail (input, length, offset, !_mm_testz_si128 (incomplete, incomplete));
}

static AVX2_TARGET gsize
utf8_invalid_index_avx2 (const guint8 *input, gsize length)
{
	const __m256i high = _mm256_broadcastsi128_si256 (_mm_loadu_si128 ((const __m128i *) utf8_high));
	const __m256i low = _mm256_broadcastsi128_si256 (_mm_loadu_si128 ((const __m128i *) utf8_low));
	const __m256i next = _mm256_broadcastsi128_si256 (_mm_loadu_si128 ((const __m128i *) utf8_current));
	const __m256i max = _mm256_loadu_si256 ((const __m256i *) utf8_max);
	const __m256i nibble = _mm256_set1_epi8 (15), high_bit = _mm256_set1_epi8 ((char) 0x80);
	const __m256i third = _mm256_set1_epi8 (0x60), fourth = _mm256_set1_epi8 (0x70);
	__m256i previous = _mm256_setzero_si256 (), incomplete = _mm256_setzero_si256 ();
	gsize offset = 0;
	gboolean pending;

	while (length - offset >= 32) {
		__m256i current = _mm256_loadu_si256 ((const __m256i *) (input + offset));

		if (!_mm256_movemask_epi8 (current)) {
			if (!_mm256_testz_si256 (incomplete, incomplete))
				break;

			incomplete = _mm256_setzero_si256 ();
		} else {
			__m256i bridge = _mm256_permute2x128_si256 (previous, current, 0x21);
			__m256i p1 = _mm256_alignr_epi8 (current, bridge, 15);
			__m256i p2 = _mm256_alignr_epi8 (current, bridge, 14);
			__m256i p3 = _mm256_alignr_epi8 (current, bridge, 13);
			__m256i h = _mm256_shuffle_epi8 (high, _mm256_and_si256 (_mm256_srli_epi16 (p1, 4), nibble));
			__m256i l = _mm256_shuffle_epi8 (low, _mm256_and_si256 (p1, nibble));
			__m256i n = _mm256_shuffle_epi8 (next, _mm256_and_si256 (_mm256_srli_epi16 (current, 4), nibble));
			__m256i required = _mm256_and_si256 (high_bit,
				_mm256_or_si256 (_mm256_subs_epu8 (p2, third), _mm256_subs_epu8 (p3, fourth)));
			__m256i errors = _mm256_xor_si256 (_mm256_and_si256 (_mm256_and_si256 (h, l), n), required);

			if (!_mm256_testz_si256 (errors, errors))
				break;

			incomplete = _mm256_subs_epu8 (current, max);
		}

		previous = current;
		offset += 32;
	}

	pending = !_mm256_testz_si256 (incomplete, incomplete);
	_mm256_zeroupper ();

	return utf8_invalid_index_tail (input, length, offset, pending);
}

static gboolean
have_sse41 (void)
{
#ifdef _MSC_VER
	int info[4];

	__cpuid (info, 1);
	return (info[2] & ((1 << 9) | (1 << 19))) == ((1 << 9) | (1 << 19));
#else
	__builtin_cpu_init ();
	return __builtin_cpu_supports ("ssse3") && __builtin_cpu_supports ("sse4.1");
#endif
}
#endif

#if defined(HAVE_READER_NEON) && (defined(__aarch64__) || defined(_M_ARM64))
static gsize
utf8_invalid_index_neon (const guint8 *input, gsize length)
{
	const uint8x16_t high = vld1q_u8 (utf8_high), low = vld1q_u8 (utf8_low);
	const uint8x16_t next = vld1q_u8 (utf8_current), max = vld1q_u8 (utf8_max + 16);
	const uint8x16_t nibble = vdupq_n_u8 (15), high_bit = vdupq_n_u8 (128);
	const uint8x16_t third = vdupq_n_u8 (0x60), fourth = vdupq_n_u8 (0x70);
	uint8x16_t previous = vdupq_n_u8 (0), incomplete = vdupq_n_u8 (0);
	gsize offset = 0;

	while (length - offset >= 16) {
		uint8x16_t current = vld1q_u8 (input + offset);

		if (vmaxvq_u8 (current) < 128) {
			if (vmaxvq_u8 (incomplete))
				break;

			incomplete = vdupq_n_u8 (0);
		} else {
			uint8x16_t p1 = vextq_u8 (previous, current, 15);
			uint8x16_t p2 = vextq_u8 (previous, current, 14);
			uint8x16_t p3 = vextq_u8 (previous, current, 13);
			uint8x16_t h = vqtbl1q_u8 (high, vshrq_n_u8 (p1, 4));
			uint8x16_t l = vqtbl1q_u8 (low, vandq_u8 (p1, nibble));
			uint8x16_t n = vqtbl1q_u8 (next, vshrq_n_u8 (current, 4));
			uint8x16_t required = vandq_u8 (high_bit, vorrq_u8 (vqsubq_u8 (p2, third), vqsubq_u8 (p3, fourth)));
			uint8x16_t errors = veorq_u8 (vandq_u8 (vandq_u8 (h, l), n), required);

			if (vmaxvq_u8 (errors))
				break;

			incomplete = vqsubq_u8 (current, max);
		}

		previous = current;
		offset += 16;
	}

	return utf8_invalid_index_tail (input, length, offset, vmaxvq_u8 (incomplete) != 0);
}
#endif

GMimeReaderUtf8Func
_g_mime_reader_utf8_get_backend (GMimeReaderUtf8Backend backend)
{
	switch (backend) {
	case GMIME_READER_UTF8_SCALAR:
		return utf8_invalid_index_scalar;
	case GMIME_READER_UTF8_SSE41:
#ifdef HAVE_READER_SSE2
		if (have_sse41 ())
			return utf8_invalid_index_sse41;
#endif
		return NULL;
	case GMIME_READER_UTF8_AVX2:
#ifdef HAVE_READER_AVX2
		if (have_avx2 ())
			return utf8_invalid_index_avx2;
#endif
		return NULL;
	case GMIME_READER_UTF8_NEON:
#if defined(HAVE_READER_NEON) && (defined(__aarch64__) || defined(_M_ARM64)) && G_BYTE_ORDER == G_LITTLE_ENDIAN
		return utf8_invalid_index_neon;
#else
		return NULL;
#endif
	default:
		g_return_val_if_reached (NULL);
	}
}

gsize
_g_mime_reader_utf8_invalid_index (const guint8 *input, gsize length)
{
	static GMimeReaderUtf8Func validator;
	static gsize initialized;

	if (length < 128)
		return utf8_invalid_index_scalar (input, length);

	if (g_once_init_enter (&initialized)) {
		validator = _g_mime_reader_utf8_get_backend (GMIME_READER_UTF8_AVX2);

		if (!validator)
			validator = _g_mime_reader_utf8_get_backend (GMIME_READER_UTF8_SSE41);

		if (!validator)
			validator = _g_mime_reader_utf8_get_backend (GMIME_READER_UTF8_NEON);

		if (!validator)
			validator = utf8_invalid_index_scalar;

		g_once_init_leave (&initialized, 1);
	}

	return validator (input, length);
}
