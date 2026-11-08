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
#include <stdio.h>
#include <stdlib.h>

static char *pattern;
static gboolean list, verify;
static gint milliseconds = 50;
static guint matched;
static volatile guint64 result_sink;

gboolean
benchmark_selected (const char *name)
{
	if (pattern && !g_pattern_match_simple (pattern, name))
		return FALSE;

	matched++;

	if (list)
		puts (name);

	return !list;
}

static gint
compare_times (gconstpointer a, gconstpointer b)
{
	double x = *(const double *) a, y = *(const double *) b;

	return x < y ? -1 : x > y;
}

void
benchmark_run (const char *name, gsize bytes, BenchmarkFunc func, gpointer data)
{
	guint64 expected = func (data), result = 0, iterations, total = 0;
	gint64 start, elapsed;
	double samples[5];
	guint round;

	if (verify) {
		if (func (data) != expected)
			g_error ("%s: unstable result across repeated operations", name);

		printf ("verified %s (%" G_GSIZE_FORMAT " input bytes, result %" G_GUINT64_FORMAT ")\n",
			name, bytes, expected);
		return;
	}

	start = g_get_monotonic_time ();
	do {
		result_sink = func (data);
	} while (g_get_monotonic_time () - start < 5000);

	for (round = 0; round < G_N_ELEMENTS (samples); round++) {
		iterations = 0;
		start = g_get_monotonic_time ();

		do {
			guint batch;

			for (batch = 0; batch < 16; batch++)
				result = func (data);

			iterations += 16;
			elapsed = g_get_monotonic_time () - start;
		} while (elapsed < (gint64) milliseconds * 1000);

		if (result != expected)
			g_error ("%s: result changed during measurement", name);

		result_sink = result;
		samples[round] = elapsed * 1000.0 / iterations;
		total += iterations;
	}

	qsort (samples, G_N_ELEMENTS (samples), sizeof (double), compare_times);

	printf ("%s,%" G_GSIZE_FORMAT ",%" G_GUINT64_FORMAT ",%.1f,%.2f,%" G_GUINT64_FORMAT "\n",
		name, bytes, total, samples[2], bytes * 1e9 / samples[2] / (1024 * 1024), expected);
}

GBytes *
benchmark_load (const char *directory, const char *name)
{
	char *path = g_build_filename (directory, name, NULL), *data;
	GError *error = NULL;
	gsize length;

	if (!g_file_get_contents (path, &data, &length, &error))
		g_error ("Cannot read benchmark fixture %s: %s", path, error->message);

	g_free (path);

	return g_bytes_new_take (data, length);
}

GBytes *
benchmark_newlines (GBytes *bytes, gboolean dos)
{
	gsize length, i;
	const char *data = g_bytes_get_data (bytes, &length);
	GString *text = g_string_sized_new (length);

	for (i = 0; i < length; i++) {
		if (data[i] == '\r' && i + 1 < length && data[i + 1] == '\n')
			continue;

		if (data[i] == '\n' && dos)
			g_string_append_c (text, '\r');

		g_string_append_c (text, data[i]);
	}

	return g_string_free_to_bytes (text);
}

void
benchmark_equal (GBytes *actual, GBytes *expected, const char *name)
{
	if (!g_bytes_equal (actual, expected))
		g_error ("%s: output differs from the reference", name);
}

void
benchmark_check (gboolean success, const char *operation, GError **error)
{
	if (!success || *error)
		g_error ("%s: %s", operation, *error ? (*error)->message : "operation failed without an error");
}

int
main (int argc, char **argv)
{
	char *datadir = NULL, *testdatadir = NULL;
	GOptionEntry entries[] = {
		{ "filter", 'f', 0, G_OPTION_ARG_STRING, &pattern, "Select names using a glob", "PATTERN" },
		{ "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List matching workloads", NULL },
		{ "verify", 0, 0, G_OPTION_ARG_NONE, &verify, "Check workloads without timing", NULL },
		{ "milliseconds", 0, 0, G_OPTION_ARG_INT, &milliseconds, "Minimum duration of each of five samples", "MS" },
		{ "data", 0, 0, G_OPTION_ARG_FILENAME, &datadir, "Path to benchmarks/data", "DIRECTORY" },
		{ "test-data", 0, 0, G_OPTION_ARG_FILENAME, &testdatadir, "Path to shared tests/data fixtures", "DIRECTORY" },
		{ NULL }
	};
	GOptionContext *options = g_option_context_new ("- portable MimeKit benchmark workloads");
	GError *error = NULL;
	int status = 0;

	g_option_context_add_main_entries (options, entries, NULL);

	if (!g_option_context_parse (options, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_clear_error (&error);
		status = 1;
		goto done;
	}

	if (argc != 1 || milliseconds < 1 || milliseconds > 60000) {
		g_printerr ("Unexpected arguments or --milliseconds outside 1..60000.\n");
		status = 1;
		goto done;
	}

	if (!datadir)
		datadir = g_strdup ("benchmarks" G_DIR_SEPARATOR_S "data");

	if (!testdatadir)
		testdatadir = g_strdup ("tests" G_DIR_SEPARATOR_S "data");

	g_mime_init ();

	if (!list && !verify)
		puts ("name,input_bytes,iterations,median_ns_per_operation,MiB_per_second,result");

	benchmark_headers ();
	benchmark_parser (datadir, testdatadir);
	benchmark_codecs (datadir, testdatadir);

	g_mime_shutdown ();

	if (!matched) {
		g_printerr ("No benchmarks matched.\n");
		status = 1;
	}

done:
	g_option_context_free (options);
	g_free (pattern);
	g_free (datadir);
	g_free (testdatadir);

	return status;
}
