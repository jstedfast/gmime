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

static GMimeReaderLineScanFunc scanners[5];
static GMimeReaderUtf8Func validators[5];
static guint scanner_count, validator_count;

static void
init_backends (void)
{
	static const char *names[] = { "scalar", "SSE2", "AVX2", "NEON" };
	static const char *utf8_names[] = { "scalar", "SSSE3/SSE4.1", "AVX2", "NEON" };
	guint i;
#ifndef GMIME_DISABLE_SIMD
	const char *required = g_getenv ("GMIME_TEST_REQUIRE_READER_BACKEND");
	gboolean found = required == NULL;
#endif

	scanners[scanner_count++] = _g_mime_reader_scan_line;
	validators[validator_count++] = _g_mime_reader_utf8_invalid_index;
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		GMimeReaderLineScanFunc scanner = _g_mime_reader_scan_line_get_backend (i);
		GMimeReaderUtf8Func validator = _g_mime_reader_utf8_get_backend (i);

		g_print ("%s byte scanner: %s; %s UTF-8 validator: %s\n", names[i], scanner ? "enabled" : "unavailable",
			 utf8_names[i], validator ? "enabled" : "unavailable");
		if (scanner)
			scanners[scanner_count++] = scanner;
		if (validator)
			validators[validator_count++] = validator;
#ifndef GMIME_DISABLE_SIMD
		if (required && !strcmp (required, names[i])) {
			g_assert_nonnull (scanner);
			g_assert_nonnull (validator);
			found = TRUE;
		}
#endif
	}
#ifdef GMIME_DISABLE_SIMD
	g_assert_cmpuint (scanner_count, ==, 2);
	g_assert_cmpuint (validator_count, ==, 2);
#else
	g_assert_true (found);
#endif
}

static void
check_line (const guint8 *input, gsize length)
{
	guint flags, backend;
	gsize i;

	for (flags = 0; flags < 4; flags++) {
		GMimeReaderLineScan expected = { length, G_MAXSIZE, G_MAXSIZE };

		for (i = 0; i < length; i++) {
			if (input[i] == '\n') {
				expected.length = i;
				break;
			}
			if ((flags & 1) && input[i] == 0 && expected.null_index == G_MAXSIZE)
				expected.null_index = i;
			if ((flags & 2) && input[i] >= 128 && expected.eight_bit_index == G_MAXSIZE)
				expected.eight_bit_index = i;
		}
		for (backend = 0; backend < scanner_count; backend++) {
			GMimeReaderLineScan actual = scanners[backend] (input, length, flags & 1, flags & 2);

			g_assert_cmpuint (actual.length, ==, expected.length);
			g_assert_cmpuint (actual.null_index, ==, expected.null_index);
			g_assert_cmpuint (actual.eight_bit_index, ==, expected.eight_bit_index);
		}
	}
}

static void
check_utf8 (const guint8 *input, gsize length)
{
	gsize invalid = 0;
	guint backend;

	while (invalid < length) {
		gunichar c;

		if (input[invalid] < 128) {
			invalid++;
			continue;
		}
		c = g_utf8_get_char_validated ((const char *) input + invalid, length - invalid);
		if (c == (gunichar) -1 || c == (gunichar) -2)
			break;
		invalid += g_utf8_skip[input[invalid]];
	}
	for (backend = 0; backend < validator_count; backend++)
		g_assert_cmpuint (validators[backend] (input, length), ==, invalid);
}

static void
test_line_edges (void)
{
	guint8 storage[384];
	guint alignment, lf, kind, length;

	check_line (NULL, 0);
	for (alignment = 0; alignment < 64; alignment++) {
		guint8 *input = storage + alignment;

		for (lf = 0; lf <= 256; lf++) {
			for (kind = 0; kind < 4; kind++) {
				memset (input, 'a', 300);
				input[lf] = '\n';
				input[lf + 1] = 0;
				input[lf + 2] = 0xff;
				if (lf && kind) {
					input[lf - 1] = kind & 1 ? 0 : 0xff;
					if (lf > 1 && kind == 3)
						input[lf - 2] = 0xff;
					if (lf > 64) {
						input[2] = 0;
						input[3] = 0x80;
					}
				}
				check_line (input, 300);
				check_line (input, lf);
				check_line (input, lf + 1);
			}
		}
		for (kind = 0; kind < 3; kind++) {
			memset (input, kind == 0 ? 'a' : kind == 1 ? 0 : 0x80, 300);
			for (length = 0; length <= 300; length++)
				check_line (input, length);
		}
	}
}

static void
test_utf8_edges (void)
{
	static const char *sequences[] = {
		"\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf", "\xef\xbf\xbf",
		"\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf", "\x80", "\xc0\x80", "\xc1\xbf",
		"\xc2", "\xc2\x7f", "\xe0\x9f\xbf", "\xed\xa0\x80", "\xe2\x82",
		"\xe2\x28\xa1", "\xe2\x82\x20", "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80",
		"\xf5\x80\x80\x80", "\xf0\x90\x80", "\xf0\x90\x80\x7f", "\xff"
	};
	guint8 storage[640];
	guint alignment, index, sequence, tail, pair;

	check_utf8 (NULL, 0);
	for (alignment = 0; alignment < 32; alignment++) {
		guint8 *input = storage + alignment;

		for (index = 0; index < 260; index++) {
			for (sequence = 0; sequence < G_N_ELEMENTS (sequences); sequence++) {
				gsize length = strlen (sequences[sequence]);

				memset (input, 'a', 600);
				memcpy (input + index, sequences[sequence], length);
				check_utf8 (input, 600);
				for (tail = 0; tail <= length; tail++)
					check_utf8 (input, index + tail);
				input[index + length] = 0;
				check_utf8 (input, index + length + 1);
			}
		}
	}
	memset (storage, 'a', sizeof (storage));
	for (pair = 0; pair <= 65535; pair++) {
		storage[31] = pair >> 8;
		storage[32] = pair;
		check_utf8 (storage, sizeof (storage));
	}
}

static void
test_utf8_runs (void)
{
	static const char *patterns[] = { "a", "\xc3\xa9", "\xe6\x97\xa5", "\xf0\x9f\x98\x80",
					 "ascii \xc3\xa9\xe6\x97\xa5\xf0\x9f\x98\x80" };
	static const guint8 replacements[] = { 0, 0x7f, 0x80, 0xbf, 0xc0, 0xc2, 0xdf, 0xe0, 0xed, 0xef, 0xf0, 0xf4, 0xf5, 0xff };
	guint8 storage[1056];
	guint pattern, alignment, length, index, replacement;

	for (pattern = 0; pattern < G_N_ELEMENTS (patterns); pattern++) {
		gsize size = strlen (patterns[pattern]);

		for (alignment = 0; alignment < 32; alignment++) {
			guint8 *input = storage + alignment;

			memset (storage, 0xff, sizeof (storage));
			for (index = 0; index < 1024; index++)
				input[index] = patterns[pattern][index % size];
			for (length = 0; length <= 1024; length++)
				check_utf8 (input, length);
		}
		for (index = 0; index < 512; index++)
			storage[index] = patterns[pattern][index % size];
		for (index = 0; index < 512; index++) {
			guint8 saved = storage[index];

			for (replacement = 0; replacement < G_N_ELEMENTS (replacements); replacement++) {
				storage[index] = replacements[replacement];
				check_utf8 (storage, 512);
			}
			storage[index] = saved;
		}
	}
}

static void
test_differential (void)
{
	static const guint8 valid[] = "plain\0\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\r\n";
	GRand *random = g_rand_new_with_seed (0x783d008);
	guint8 data[4096];
	guint iteration, i, length;

	for (iteration = 0; iteration < 12000; iteration++) {
		length = g_rand_int_range (random, 0, sizeof (data) + 1);
		for (i = 0; i < length; i++)
			data[i] = iteration & 1 ? valid[i % (sizeof (valid) - 1)] : g_rand_int (random);
		if (length && iteration % 3 == 0)
			data[g_rand_int_range (random, 0, length)] = g_rand_int (random);
		check_line (data, length);
		check_utf8 (data, length);
	}
	g_rand_free (random);
}

static void
test_guard_pages (void)
{
#ifndef G_OS_WIN32
	gsize page = sysconf (_SC_PAGESIZE), length;
	guint8 *pages = mmap (NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	guint layout, kind;

	g_assert_true (pages != MAP_FAILED);
	g_assert_cmpint (mprotect (pages + page, page, PROT_READ | PROT_WRITE), ==, 0);
	for (layout = 0; layout < 2; layout++) {
		for (length = 0; length <= MIN (page, 512); length++) {
			guint8 *input = layout ? pages + page : pages + page * 2 - length;

			for (kind = 0; kind < 4; kind++) {
				gsize i;

				for (i = 0; i < length; i++)
					input[i] = kind == 0 ? 'a' : kind == 1 ? 0 : i & 1 ? 0xa9 : 0xc3;
				if (length && kind == 3)
					input[length - 1] = '\n';
				check_line (input, length);
				check_utf8 (input, length);
			}
		}
	}
	g_assert_cmpint (munmap (pages, page * 3), ==, 0);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

static GMimeReaderLineScan
scan_line_separate (const guint8 *input, gsize length, gboolean detect_null, gboolean detect_eight_bit)
{
	const guint8 *lf = memchr (input, '\n', length), *nul;
	GMimeReaderLineScan scan = { lf ? (gsize) (lf - input) : length, G_MAXSIZE, G_MAXSIZE };
	gsize i;

	if (detect_eight_bit) {
		for (i = 0; i < scan.length && input[i] < 128; i++)
			;
		if (i < scan.length)
			scan.eight_bit_index = i;
	}
	if (detect_null && (nul = memchr (input, 0, scan.length)))
		scan.null_index = nul - input;

	return scan;
}

static void
benchmark (void)
{
	static const char *patterns[] = { "a", "\xc3\xa9", "\xf0\x9f\x98\x80" };
	GMimeReaderLineScanFunc lines[] = { scan_line_separate, _g_mime_reader_scan_line };
	GMimeReaderUtf8Func utf8[] = {
		_g_mime_reader_utf8_get_backend (GMIME_READER_UTF8_SCALAR), _g_mime_reader_utf8_invalid_index
	};
	guint8 data[4096];
	volatile guint64 total = 0;
	guint backend, repeat, pattern, i;
	double elapsed[2];

	memset (data, 'a', sizeof (data));
	data[sizeof (data) - 1] = '\n';
	for (backend = 0; backend < 2; backend++) {
		gint64 start = g_get_monotonic_time ();

		for (repeat = 0; repeat < 50000; repeat++)
			total += lines[backend] (data, sizeof (data), TRUE, TRUE).length;
		elapsed[backend] = (g_get_monotonic_time () - start) / 1000000.0;
	}
	g_print ("Compliance line scanning: separate %.1f MB/s, dispatched %.1f MB/s (%.2fx)\n",
		 204.8 / elapsed[0], 204.8 / elapsed[1], elapsed[0] / elapsed[1]);
	for (pattern = 0; pattern < G_N_ELEMENTS (patterns); pattern++) {
		gsize size = strlen (patterns[pattern]);

		for (i = 0; i < sizeof (data); i++)
			data[i] = patterns[pattern][i % size];
		for (backend = 0; backend < 2; backend++) {
			gint64 start = g_get_monotonic_time ();

			for (repeat = 0; repeat < 50000; repeat++)
				total += utf8[backend] (data, sizeof (data));
			elapsed[backend] = (g_get_monotonic_time () - start) / 1000000.0;
		}
		g_print ("UTF-8 (%" G_GSIZE_FORMAT "-byte characters): scalar %.1f MB/s, dispatched %.1f MB/s (%.2fx)\n",
			 size, 204.8 / elapsed[0], 204.8 / elapsed[1], elapsed[0] / elapsed[1]);
	}
	g_assert_cmpuint (total, >, 0);
}

int
main (int argc, char **argv)
{
	if (argc > 1 && !strcmp (argv[1], "--benchmark")) {
		init_backends ();
		benchmark ();
		return 0;
	}
	g_test_init (&argc, &argv, NULL);
	init_backends ();
	g_test_add_func ("/reader-bytes/line-edges", test_line_edges);
	g_test_add_func ("/reader-bytes/utf8-edges", test_utf8_edges);
	g_test_add_func ("/reader-bytes/utf8-runs", test_utf8_runs);
	g_test_add_func ("/reader-bytes/differential", test_differential);
	g_test_add_func ("/reader-bytes/guard-pages", test_guard_pages);
	return g_test_run ();
}
