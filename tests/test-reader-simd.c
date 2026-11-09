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
#include <string.h>
#include <gmime/gmime-reader-simd-private.h>
#ifndef G_OS_WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

static GMimeReaderScanFunc scanners[5];
static GMimeReaderContentScanFunc content_scanners[5];
static const char *scanner_names[5];
static guint scanner_count;

static void
init_scanners (void)
{
	static const char *names[] = { "scalar", "SSE2", "AVX2", "NEON" };
	GMimeReaderScanBackend backend;
#ifndef GMIME_DISABLE_SIMD
	const char *required = g_getenv ("GMIME_TEST_REQUIRE_READER_BACKEND");
	gboolean found = required == NULL;
#endif

	scanner_names[scanner_count] = "dispatched";
	content_scanners[scanner_count] = _g_mime_reader_scan_content;
	scanners[scanner_count++] = _g_mime_reader_scan_lines;
	for (backend = GMIME_READER_SCAN_SCALAR; backend <= GMIME_READER_SCAN_NEON; backend++) {
		GMimeReaderScanFunc scanner = _g_mime_reader_scan_lines_get_backend (backend);

		if (scanner) {
			g_print ("Checking MIME scanner backend: %s\n", names[backend]);
			scanner_names[scanner_count] = names[backend];
			content_scanners[scanner_count] = _g_mime_reader_scan_content_get_backend (backend);
			g_assert_nonnull (content_scanners[scanner_count]);
			scanners[scanner_count++] = scanner;
		}
		else {
			g_print ("Skipping MIME scanner backend: %s (not compiled or CPU/OS unsupported)\n", names[backend]);
		}
#ifndef GMIME_DISABLE_SIMD
		if (required && !strcmp (required, names[backend])) {
			if (!scanner)
				g_error ("Required MIME scanner backend is unavailable: %s", required);
			found = TRUE;
		}
#endif
#ifdef GMIME_DISABLE_SIMD
		if (!scanner)
			g_assert_cmpint (backend, !=, GMIME_READER_SCAN_SCALAR);
#endif
	}
#ifndef GMIME_DISABLE_SIMD
	if (!found)
		g_error ("Unknown required MIME scanner backend: %s", required);
#endif
#ifdef GMIME_DISABLE_SIMD
	g_assert_cmpuint (scanner_count, ==, 2);
#elif defined(__x86_64__) || defined(_M_X64)
	g_assert_nonnull (_g_mime_reader_scan_lines_get_backend (GMIME_READER_SCAN_SSE2));
#elif defined(__aarch64__) || defined(_M_ARM64)
	g_assert_nonnull (_g_mime_reader_scan_lines_get_backend (GMIME_READER_SCAN_NEON));
#endif
}

static void
check (const guint8 *data, gsize length, guint8 first, guint8 second)
{
	GMimeReaderScan expected = { 0 };
	gsize i;
	guint backend;

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
	for (backend = 0; backend < scanner_count; backend++) {
		GMimeReaderScan actual = scanners[backend] (data, length, first, second);

		g_assert_cmpuint (actual.length, ==, expected.length);
		g_assert_cmpuint (actual.previous_line, ==, expected.previous_line);
		g_assert_cmpuint (actual.lines, ==, expected.lines);
		g_assert_cmpuint (actual.formats, ==, expected.formats);
	}
}

static void
check_vector_edges (void)
{
	static const char *patterns[] = { "a", "\n", "\r\n", "\r\n\n" };
	static const guint8 prefixes[][2] = { { '-', '-' }, { 'F', 'r' }, { 1, 1 }, { 0x80, 0xff }, { 0, 0 } };
	guint8 storage[256 + 64];
	guint alignment, kind, i, lf, prefix;

	check (NULL, 0, 'F', 'r');
	for (alignment = 0; alignment < 64; alignment++) {
		guint8 *data = storage + alignment;

		for (kind = 0; kind < G_N_ELEMENTS (patterns); kind++) {
			gsize pattern_length = strlen (patterns[kind]);

			for (i = 0; i < 256; i++)
				data[i] = patterns[kind][i % pattern_length];
			for (i = 0; i <= 256; i++)
				check (data, i, 'F', 'r');
		}
		for (prefix = 0; prefix < G_N_ELEMENTS (prefixes); prefix++) {
			for (lf = 0; lf < 96; lf++) {
				for (kind = 0; kind < 3; kind++) {
					memset (data, kind == 2 ? '\n' : 'a', 256);
					if (lf > 4)
						data[3] = '\n';
					if (kind == 1 && lf)
						data[lf - 1] = '\r';
					data[lf] = '\n';
					data[lf + 1] = prefixes[prefix][0];
					data[lf + 2] = prefixes[prefix][1];
					data[lf + 8] = '\n';
					check (data, 256, prefixes[prefix][0], prefixes[prefix][1]);
					check (data, lf + 1, prefixes[prefix][0], prefixes[prefix][1]);
					check (data, lf + 2, prefixes[prefix][0], prefixes[prefix][1]);
					check (data, lf + 3, prefixes[prefix][0], prefixes[prefix][1]);
					data[lf + 2] ^= 0x20;
					check (data, 256, prefixes[prefix][0], prefixes[prefix][1]);
					data[lf + 1] = data[lf + 2] = '-';
					check (data, 256, prefixes[prefix][0], prefixes[prefix][1]);
				}
			}
		}
	}
}

static void
check_content (const guint8 *data, gsize length, const GMimeReaderContentScanOptions *options)
{
	GMimeReaderContentScanFunc scalar = _g_mime_reader_scan_content_get_backend (GMIME_READER_SCAN_SCALAR);
	GMimeReaderContentScan limit = scalar (data, length, options);
	guint backend;

	for (backend = 0; backend < scanner_count; backend++) {
		GMimeReaderContentScan actual = content_scanners[backend] (data, length, options);
		GMimeReaderContentScan expected = scalar (data, actual.scan.length, options);
		gsize i;

		g_assert_cmpuint (actual.scan.length, <=, limit.scan.length);
		if (options->encoding == GMIME_CONTENT_ENCODING_UUENCODE)
			g_assert_cmpuint (actual.scan.length, ==, limit.scan.length);
		g_assert_cmpuint (actual.examined, >=, actual.scan.length);
		g_assert_cmpuint (actual.examined, <=, length);
		g_assert_cmpuint (actual.scan.length, ==, expected.scan.length);
		g_assert_cmpuint (actual.scan.previous_line, ==, expected.scan.previous_line);
		g_assert_cmpuint (actual.scan.lines, ==, expected.scan.lines);
		g_assert_cmpuint (actual.scan.formats, ==, expected.scan.formats);
		g_assert_cmpuint (actual.alphabet_count, ==, expected.alphabet_count);
		for (i = actual.scan.length; i < actual.examined; i++) {
			g_assert_cmpuint (data[i], !=, '\n');
			if (options->detect_null)
				g_assert_cmpuint (data[i], !=, 0);
			if (options->detect_eight_bit)
				g_assert_cmpuint (data[i], <, 128);
		}
	}
}

static void
check_content_edges (void)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE
	};
	static const char *patterns[] = { "ABCDEF\r\n", "=41=ab=\r\n", "\r\n", "A\r\n--b\r\n", "A\r\nFrom a\r\n" };
	static const guint columns[] = { 0, 1, 970, 998, 999, 1000, 4096 };
	GMimeReaderContentScanOptions options = { '-', '-', TRUE, TRUE, FALSE, 0, 0, 0 };
	guint8 storage[320];
	guint kind, pattern, alignment, value, lane, column, flags;
	gsize length, i;

	for (kind = 0; kind < G_N_ELEMENTS (encodings); kind++) {
		options.encoding = encodings[kind];
		check_content (NULL, 0, &options);
		for (alignment = 0; alignment < 32; alignment++) {
			guint8 *data = storage + alignment;

			for (pattern = 0; pattern < G_N_ELEMENTS (patterns); pattern++) {
				gsize size = strlen (patterns[pattern]);

				for (i = 0; i < 256; i++)
					data[i] = patterns[pattern][i % size];
				for (length = 0; length <= 256; length++) {
					options.midline_cr = (length & 1) != 0;
					options.first = length & 2 ? 'F' : '-';
					options.second = length & 2 ? 'r' : '-';
					check_content (data, length, &options);
				}
			}
		}
		options.first = options.second = '-';
		for (value = 0; value < 256; value++) {
			for (lane = 0; lane < 64; lane++) {
				for (i = 0; i < sizeof storage; i++)
					storage[i] = "AAAAAA\r\n"[i % 8];
				storage[64 + lane] = value;
				if (kind == 2 && lane)
					storage[63 + lane] = '=';
				check_content (storage, sizeof storage, &options);
			}
		}
		for (column = 0; column < G_N_ELEMENTS (columns); column++) {
			options.line_length = columns[column];
			for (flags = 0; flags < 4; flags++) {
				options.detect_null = flags & 1;
				options.detect_eight_bit = flags & 2;
				memset (storage, 'A', sizeof storage);
				storage[33] = '\r';
				storage[34] = '\n';
				storage[100] = 0;
				storage[200] = 0xff;
				check_content (storage, sizeof storage, &options);
			}
		}
		options.line_length = 0;
		options.detect_null = options.detect_eight_bit = TRUE;
	}

	options.encoding = GMIME_CONTENT_ENCODING_UUENCODE;
	for (value = 1; value <= 45; value++) {
		gsize count = 4 * ((value + 2) / 3);

		storage[0] = 32 + value;
		memset (storage + 1, '!', count);
		storage[count + 1] = '\r';
		storage[count + 2] = '\n';
		memcpy (storage + count + 3, storage, count + 3);
		for (lane = 0; lane <= count + 2; lane++) {
			options.remaining = lane ? count - (lane - 1) : -1;
			options.midline_cr = lane == count + 2;
			options.line_length = lane;
			check_content (storage + lane, 2 * (count + 3) - lane, &options);
		}
	}
	options.remaining = -1;
	options.midline_cr = FALSE;
	options.line_length = 0;
	for (value = 0; value < 256; value++) {
		for (lane = 0; lane < 60; lane++) {
			storage[0] = 'M';
			memset (storage + 1, '!', 60);
			storage[61] = '\r';
			storage[62] = '\n';
			memcpy (storage + 63, storage, 63);
			storage[1 + lane] = value;
			check_content (storage, 126, &options);
		}
	}
}

static void
check_content_progress (gboolean benchmark)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_UNSPECIFIED, GMIME_CONTENT_ENCODING_BASE64,
		GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	static const char *lines[] = {
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\r\n",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\r\n",
		"plain=20text=3Dvalue=c3=a9=\r\n",
		"M!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n"
	};
	GMimeReaderContentScanOptions options = { '-', '-', TRUE, TRUE, FALSE, 0, 0, -1 };
	guint8 data[8192];
	guint kind, backend;
	gsize size, length;

	for (kind = 0; kind < G_N_ELEMENTS (encodings); kind++) {
		options.encoding = encodings[kind];
		size = strlen (lines[kind]);
		for (length = 0; length + size <= sizeof data; length += size)
			memcpy (data + length, lines[kind], size);
		check_content (data, length, &options);
		for (backend = 0; backend < scanner_count; backend++) {
			GMimeReaderContentScan scan = content_scanners[backend] (data, length, &options);

			g_assert_cmpuint (scan.scan.length, >, length - size - 32);
			g_assert_cmpuint (scan.scan.lines, >, 100);
			if (benchmark) {
				guint64 total = 0;
				gint64 start = g_get_monotonic_time (), elapsed;
				guint repeat;

				for (repeat = 0; repeat < 20000; repeat++)
					total += content_scanners[backend] (data, length, &options).scan.length;
				elapsed = g_get_monotonic_time () - start;
				g_assert_cmpuint (total, >, 0);
				g_print ("Fused content scanning (encoding %d, %s): %.1f MB/s\n",
					 encodings[kind], scanner_names[backend], length * 20000.0 / MAX (elapsed, 1));
			}
		}
	}
}

static void
benchmark (void)
{
	static const char *names[] = { "short lines", "80-byte lines", "incomplete line", "dense newlines" };
	guint8 bytes[4096];
	guint kind, i, repeat, backend;
	volatile gsize total = 0;

	for (kind = 0; kind < G_N_ELEMENTS (names); kind++) {
		gint64 start;
		double scalar, dispatched;

		memset (bytes, kind == 3 ? '\n' : 'a', sizeof bytes);
		if (kind < 2) {
			for (i = kind ? 79 : 7; i < sizeof bytes; i += kind ? 80 : 8)
				bytes[i] = '\n';
		}
		start = g_get_monotonic_time ();
		for (repeat = 0; repeat < 200000; repeat++)
			total += _g_mime_reader_scan_lines_scalar (bytes, sizeof bytes, 'F', 'r').length;
		scalar = (g_get_monotonic_time () - start) / 1000000.0;
		for (backend = 0; backend < scanner_count; backend++) {
			if (scanners[backend] == _g_mime_reader_scan_lines_scalar)
				continue;
			start = g_get_monotonic_time ();
			for (repeat = 0; repeat < 200000; repeat++)
				total += scanners[backend] (bytes, sizeof bytes, 'F', 'r').length;
			dispatched = (g_get_monotonic_time () - start) / 1000000.0;
			g_print ("MIME line scanning (%s, %s): scalar %.1f MB/s, SIMD %.1f MB/s (%.2fx)\n",
				 names[kind], scanner_names[backend], 819.2 / scalar, 819.2 / dispatched, scalar / dispatched);
		}
	}
	g_assert_cmpuint (total, >, 0);
}

int
main (int argc, char **argv)
{
	static const guint8 alphabet[] = "abc\n\r--From \1\1\1\1";
	guint8 storage[4096 + 64];
	guint32 random = 0x8532f1;
	guint i, alignment, length, pass;

	init_scanners ();
	if (argc > 1 && strcmp (argv[1], "--benchmark") == 0) {
		check_content_progress (TRUE);
		benchmark ();
		return 0;
	}
	check_vector_edges ();
	check_content_edges ();
	check_content_progress (FALSE);
	for (pass = 0; pass < 8; pass++) {
		for (i = 0; i < sizeof storage; i++) {
			random = random * 1664525u + 1013904223u;
			storage[i] = alphabet[(random >> 24) % (sizeof alphabet - 1)];
		}
		for (alignment = 0; alignment < 64; alignment++) {
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
			guint8 *data = pages + page * 2 - length;
			GMimeReaderContentScanOptions options = { '-', '-', TRUE, TRUE, FALSE, 0,
				GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, 0 };

			check (data, length, 'F', 'r');
			check_content (data, length, &options);
			if (length) {
				data[0] = '\n';
				check (data, length, 'F', 'r');
				data[0] = 'a';
				data[length - 1] = '\n';
				check (data, length, 'F', 'r');
				check_content (data, length, &options);
				data[length - 1] = '-';
				if (length > 1)
					data[length - 2] = '\n';
				check (data, length, 'F', 'r');
				memset (data, '\n', length);
				check (data, length, 'F', 'r');
				for (i = 0; i < length; i++)
					data[i] = i & 1 ? '\n' : '\r';
				check (data, length, 'F', 'r');
				check_content (data, length, &options);
				data[length - 1] = '=';
				check_content (data, length, &options);
				memset (data, 'a', length);
			}
		}
		pages[page] = '\n';
		for (length = 0; length <= 128; length++) {
			check (pages + page, length, 'F', 'r');
		}
		g_assert_cmpint (munmap (pages, page * 3), ==, 0);
	}
#endif
	g_print ("MIME scanner backend/dispatch, vector-edge, unaligned and guard-page checks passed.\n");
	return 0;
}
