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
#endif

#include "gmime-reader-simd-private.h"

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

GMimeReaderScan
_g_mime_reader_scan_lines_scalar (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	GMimeReaderScan scan = { 0 };
	gsize i;

	for (i = 0; i < length; i++) {
		if (input[i] == '\n' && complete_line (input, length, i, first, second, &scan))
			break;
	}

	return scan;
}

#if !defined(GMIME_DISABLE_SIMD) && \
    (((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) || \
     (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))))
#define HAVE_READER_SSE2 1
#include <emmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#define SSE2_TARGET
#else
#define SSE2_TARGET __attribute__((target("sse2")))
#endif

static SSE2_TARGET GMimeReaderScan
scan_lines_sse2 (const guint8 *input, gsize length, guint8 first, guint8 second)
{
	const __m128i newline = _mm_set1_epi8 ('\n');
	GMimeReaderScan scan = { 0 };
	gsize i = 0;

	while (length - i >= 16) {
		__m128i block = _mm_loadu_si128 ((const __m128i *) (input + i));
		guint mask = (guint) _mm_movemask_epi8 (_mm_cmpeq_epi8 (block, newline));

		while (mask) {
#ifdef _MSC_VER
			unsigned long bit;

			_BitScanForward (&bit, mask);
#else
			guint bit = (guint) __builtin_ctz (mask);
#endif

			if (complete_line (input, length, i + bit, first, second, &scan))
				return scan;
			mask &= mask - 1;
		}

		i += 16;
	}

	while (i < length) {
		if (input[i] == '\n' && complete_line (input, length, i, first, second, &scan))
			break;

		i++;
	}

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
#endif

GMimeReaderScan
_g_mime_reader_scan_lines (const guint8 *input, gsize length, guint8 first, guint8 second)
{
#ifdef HAVE_READER_SSE2
	static gsize selected;

	if (g_once_init_enter (&selected))
		g_once_init_leave (&selected, have_sse2 () ? 2 : 1);

	if (selected == 2)
		return scan_lines_sse2 (input, length, first, second);
#endif
	return _g_mime_reader_scan_lines_scalar (input, length, first, second);
}
