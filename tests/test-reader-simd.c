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
#include <string.h>
#include <gmime/gmime-reader-simd-private.h>
#ifndef G_OS_WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

static void
check (const guint8 *data, gsize length, guint8 first, guint8 second)
{
	GMimeReaderScan actual = _g_mime_reader_scan_lines (data, length, first, second);
	GMimeReaderScan scalar = _g_mime_reader_scan_lines_scalar (data, length, first, second);
	GMimeReaderScan expected = { 0 };
	gsize i;

	for (i = 0; i < length; i++) {
		if (data[i] != '\n')
			continue;
		expected.previous_line = expected.length;
		expected.length = i + 1;
		expected.lines++;
		expected.formats |= i && data[i - 1] == '\r' ? 2 : 1;
		if (i + 1 < length &&
		    ((data[i + 1] == '-' && (i + 2 == length || data[i + 2] == '-')) ||
		     (data[i + 1] == first && (i + 2 == length || data[i + 2] == second))))
			break;
	}
	g_assert_cmpuint (actual.length, ==, expected.length);
	g_assert_cmpuint (actual.previous_line, ==, expected.previous_line);
	g_assert_cmpuint (actual.lines, ==, expected.lines);
	g_assert_cmpuint (actual.formats, ==, expected.formats);
	g_assert_cmpuint (scalar.length, ==, expected.length);
	g_assert_cmpuint (scalar.previous_line, ==, expected.previous_line);
	g_assert_cmpuint (scalar.lines, ==, expected.lines);
	g_assert_cmpuint (scalar.formats, ==, expected.formats);
}

static void
benchmark (void)
{
	guint8 bytes[4096];
	guint kind, i, repeat;
	volatile gsize total = 0;

	for (kind = 0; kind < 3; kind++) {
		gint64 start;
		double scalar, dispatched;

		memset (bytes, 'a', sizeof bytes);
		if (kind < 2) {
			for (i = kind ? 79 : 7; i < sizeof bytes; i += kind ? 80 : 8)
				bytes[i] = '\n';
		}
		start = g_get_monotonic_time ();
		for (repeat = 0; repeat < 200000; repeat++)
			total += _g_mime_reader_scan_lines_scalar (bytes, sizeof bytes, 'F', 'r').length;
		scalar = (g_get_monotonic_time () - start) / 1000000.0;
		start = g_get_monotonic_time ();
		for (repeat = 0; repeat < 200000; repeat++)
			total += _g_mime_reader_scan_lines (bytes, sizeof bytes, 'F', 'r').length;
		dispatched = (g_get_monotonic_time () - start) / 1000000.0;
		g_print ("MIME line scanning (%s): scalar %.1f MB/s, dispatched %.1f MB/s (%.2fx)\n",
			 kind == 0 ? "short lines" : kind == 1 ? "80-byte lines" : "incomplete line",
			 819.2 / scalar, 819.2 / dispatched, scalar / dispatched);
	}
	g_assert_cmpuint (total, >, 0);
}

int
main (int argc, char **argv)
{
	static const guint8 alphabet[] = "abc\n\r--From \1\1\1\1";
	guint8 storage[4096 + 32];
	guint32 random = 0x8532f1;
	guint i, alignment, length, pass;

	if (argc > 1 && strcmp (argv[1], "--benchmark") == 0) {
		benchmark ();
		return 0;
	}
	for (pass = 0; pass < 8; pass++) {
		for (i = 0; i < sizeof storage; i++) {
			random = random * 1664525u + 1013904223u;
			storage[i] = alphabet[(random >> 24) % (sizeof alphabet - 1)];
		}
		for (alignment = 0; alignment < 32; alignment++) {
			for (length = 0; length <= 256; length++) {
				check (storage + alignment, length, 'F', 'r');
				check (storage + alignment, length, '\1', '\1');
				check (storage + alignment, length, '-', '-');
			}
			check (storage + alignment, 4096, 'F', 'r');
		}
	}
#ifndef G_OS_WIN32
	{
		gsize page = (gsize) sysconf (_SC_PAGESIZE);
		guint8 *pages = mmap (NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		g_assert_true (pages != MAP_FAILED);
		g_assert_cmpint (mprotect (pages + page, page, PROT_READ | PROT_WRITE), ==, 0);
		memset (pages + page, 'a', page);
		for (length = 0; length <= MIN (page, 4096); length++) {
			check (pages + page * 2 - length, length, 'F', 'r');
			if (length)
				pages[page * 2 - length] = '\n';
			check (pages + page * 2 - length, length, 'F', 'r');
			if (length)
				pages[page * 2 - length] = 'a';
		}
		g_assert_cmpint (munmap (pages, page * 3), ==, 0);
	}
#endif
	g_print ("MIME scanner scalar/dispatch, unaligned and guard-page checks passed.\n");
	return 0;
}
