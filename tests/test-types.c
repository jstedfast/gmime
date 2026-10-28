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

#include <gmime/gmime.h>
#include "testsuite.h"

static void
assert_mime_type (const char *file_name, const char *expected)
{
	char *actual = g_mime_types_get_mime_type (file_name);

	g_assert_cmpstr (actual, ==, expected);
	g_free (actual);
}

static void
assert_extension (const char *mime_type, const char *expected)
{
	char *actual = NULL;
	gboolean found = g_mime_types_try_get_extension (mime_type, &actual);

	g_assert_cmpint (found, ==, expected != NULL);
	g_assert_cmpstr (actual, ==, expected);
	g_free (actual);
}

typedef struct {
	GMutex mutex;
	GCond cond;
	int ready;
	gboolean start;
} ThreadStart;

typedef struct {
	ThreadStart *start;
	int index;
} ThreadData;

static gpointer
registry_thread (gpointer user_data)
{
	ThreadData *data = user_data;
	char *mime_type = g_strdup_printf ("application/x-gmime-thread-%d", data->index);
	char *extension = g_strdup_printf (".gmime-thread-%d", data->index);
	char *file_name = g_strconcat ("file", extension, NULL);
	char *previous_type, *previous_extension;
	int i;

	g_mutex_lock (&data->start->mutex);
	data->start->ready++;
	g_cond_broadcast (&data->start->cond);
	while (!data->start->start)
		g_cond_wait (&data->start->cond, &data->start->mutex);
	g_mutex_unlock (&data->start->mutex);

	/* Race the first lookup as well as replacement of existing registry entries. */
	assert_mime_type ("file.TXT", "text/plain");
	assert_extension ("TEXT/PLAIN", ".txt");
	g_mime_types_register (mime_type, extension);
	previous_type = g_mime_types_get_mime_type (file_name);
	g_assert_true (g_mime_types_try_get_extension (mime_type, &previous_extension));
	for (i = 0; i < 2000; i++) {
		g_mime_types_register (mime_type, extension);
		assert_mime_type (file_name, mime_type);
		assert_extension (mime_type, extension);
		assert_mime_type ("file.unknown-gmime-extension", "application/octet-stream");
	}
	g_assert_cmpstr (previous_type, ==, mime_type);
	g_assert_cmpstr (previous_extension, ==, extension);

	g_free (previous_type);
	g_free (previous_extension);
	g_free (file_name);
	g_free (extension);
	g_free (mime_type);
	return NULL;
}

static void
test_concurrency (void)
{
	ThreadStart start = { 0 };
	GThread *threads[8];
	ThreadData data[G_N_ELEMENTS (threads)];
	guint i;

	g_mutex_init (&start.mutex);
	g_cond_init (&start.cond);
	for (i = 0; i < G_N_ELEMENTS (threads); i++) {
		data[i].start = &start;
		data[i].index = i;
		threads[i] = g_thread_new ("mime-types", registry_thread, &data[i]);
	}
	g_mutex_lock (&start.mutex);
	while (start.ready < G_N_ELEMENTS (threads))
		g_cond_wait (&start.cond, &start.mutex);
	start.start = TRUE;
	g_cond_broadcast (&start.cond);
	g_mutex_unlock (&start.mutex);
	for (i = 0; i < G_N_ELEMENTS (threads); i++)
		g_thread_join (threads[i]);
	g_cond_clear (&start.cond);
	g_mutex_clear (&start.mutex);
}

static void
test_builtin_mappings (void)
{
	static const struct {
		const char *extension;
		const char *mime_type;
	} mappings[] = {
		{ ".txt", "text/plain" },
		{ ".csv", "text/csv" },
		{ ".3mf", "model/3mf" },
		{ ".apng", "image/apng" },
		{ ".avif", "image/avif" },
		{ ".cbor", "application/cbor" },
		{ ".flac", "audio/flac" },
		{ ".geojson", "application/geo+json" },
		{ ".glb", "model/gltf-binary" },
		{ ".gltf", "model/gltf+json" },
		{ ".gz", "application/gzip" },
		{ ".heic", "image/heic" },
		{ ".heif", "image/heif" },
		{ ".jsonld", "application/ld+json" },
		{ ".jxl", "image/jxl" },
		{ ".mkv", "video/matroska" },
		{ ".obj", "model/obj" },
		{ ".parquet", "application/vnd.apache.parquet" },
		{ ".stl", "model/stl" },
		{ ".toml", "application/toml" },
		{ ".vtt", "text/vtt" },
		{ ".wasm", "application/wasm" },
		{ ".webmanifest", "application/manifest+json" },
		{ ".yaml", "application/yaml" },
		{ ".zst", "application/zstd" },
	};
	char *file_name, *upper;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (mappings); i++) {
		file_name = g_strconcat ("filename", mappings[i].extension, NULL);
		assert_mime_type (file_name, mappings[i].mime_type);
		upper = g_ascii_strup (file_name, -1);
		assert_mime_type (upper, mappings[i].mime_type);
		assert_extension (mappings[i].mime_type, mappings[i].extension);
		g_free (upper);
		upper = g_ascii_strup (mappings[i].mime_type, -1);
		assert_extension (upper, mappings[i].extension);
		g_free (upper);
		g_free (file_name);
	}
	assert_mime_type ("file.gzip", "application/gzip");
	assert_mime_type ("file.py", "text/x-script.python");
	assert_mime_type ("file.PY", "text/x-script.python");
	assert_extension ("text/x-script.phyton", ".py");
	assert_mime_type ("file.yml", "application/yaml");
	assert_extension ("application/x-gzip", ".gz");
	assert_extension ("audio/x-flac", ".flac");
	assert_extension ("video/x-matroska", ".mkv");
	assert_extension ("application/sla", ".stl");
	assert_extension ("image/jpeg", ".jpg");
	assert_extension ("text/html", ".html");
	assert_extension ("application/x-gmime-unknown", NULL);
	assert_extension ("", NULL);
	assert_extension (" text/plain", NULL);
	assert_extension ("text/plain ", NULL);
	assert_extension ("text/plain; charset=utf-8", NULL);
}

static void
test_paths (void)
{
	static const char *unknown[] = {
		"", "filename", "filename.", ".", "..", "filename.unknown-gmime-extension",
		"folder.txt/filename", "folder.txt/", "filename.txt ", "filename.txt?query",
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (unknown); i++)
		assert_mime_type (unknown[i], "application/octet-stream");
	assert_mime_type (".txt", "text/plain");
	assert_mime_type ("..txt", "text/plain");
	assert_mime_type ("folder/file.Txt", "text/plain");
	assert_mime_type ("folder.with.dots/archive.tar.gz", "application/gzip");
	assert_mime_type ("file.txt?query.csv", "text/csv");
	assert_mime_type ("folder/\303\251.txt", "text/plain");
	assert_mime_type ("folder\\file.txt", "text/plain");

	g_mime_types_register ("application/x-gmime-backslash", ".txt\\file");
#ifdef G_OS_WIN32
	assert_mime_type ("folder.txt\\file", "application/octet-stream");
#else
	assert_mime_type ("folder.txt\\file", "application/x-gmime-backslash");
#endif
	assert_mime_type ("folder.txt/file", "application/octet-stream");
}

static void
test_registration (void)
{
	char *mime_type = g_strdup ("application/x-gmime-one");
	char *extension = g_strdup ("gmime-one");
	char *old_type, *old_extension, *missing = "sentinel";

	g_mime_types_register (mime_type, extension);
	memset (mime_type, 'x', strlen (mime_type));
	memset (extension, 'x', strlen (extension));
	g_free (mime_type);
	g_free (extension);
	assert_mime_type ("file.GMIME-ONE", "application/x-gmime-one");
	assert_extension ("APPLICATION/X-GMIME-ONE", ".gmime-one");
	old_type = g_mime_types_get_mime_type ("file.gmime-one");
	g_assert_true (g_mime_types_try_get_extension ("application/x-gmime-one", &old_extension));

	g_mime_types_register ("application/x-gmime-one", ".GMime-Two");
	assert_extension ("application/x-gmime-one", ".GMime-Two");
	assert_mime_type ("file.gmime-one", "application/x-gmime-one");
	assert_mime_type ("file.gmime-two", "application/x-gmime-one");
	g_mime_types_register ("Application/X-GMime-Three", "GMime-Two");
	assert_mime_type ("file.gmime-two", "Application/X-GMime-Three");
	assert_extension ("application/x-gmime-three", ".GMime-Two");
	assert_extension ("application/x-gmime-one", ".GMime-Two");
	g_assert_cmpstr (old_type, ==, "application/x-gmime-one");
	g_assert_cmpstr (old_extension, ==, ".gmime-one");
	g_free (old_type);
	g_free (old_extension);

	g_assert_false (g_mime_types_try_get_extension ("application/x-gmime-absent", &missing));
	g_assert_null (missing);
	g_mime_types_register (" X-GMime/Unparsed ", "gmime-whitespace ");
	assert_mime_type ("file.GMIME-WHITESPACE ", " X-GMime/Unparsed ");
	assert_extension (" x-gmime/unparsed ", ".gmime-whitespace ");
	assert_extension ("X-GMime/Unparsed", NULL);
	g_mime_types_register ("application/x-gmime-dot", ".");
	assert_extension ("application/x-gmime-dot", ".");
	assert_mime_type ("file.", "application/octet-stream");
	g_mime_types_register ("application/x-gmime-multiple", "gmime.multi");
	assert_extension ("application/x-gmime-multiple", ".gmime.multi");
	assert_mime_type ("file.gmime.multi", "application/octet-stream");
	g_mime_types_register ("application/x-gmime-utf8", ".\303\204-gmime");
	assert_mime_type ("file.\303\204-GMIME", "application/x-gmime-utf8");
	assert_mime_type ("file.\303\244-gmime", "application/octet-stream");

	g_mime_types_register ("text/plain", ".gmime-plain");
	assert_extension ("TEXT/PLAIN", ".gmime-plain");
	assert_mime_type ("file.txt", "text/plain");
	g_mime_types_register ("application/x-gmime-txt", ".txt");
	assert_mime_type ("file.txt", "application/x-gmime-txt");
	assert_extension ("TEXT/PLAIN", ".gmime-plain");
	g_mime_types_register ("text/plain", ".txt");
}

static void
test_preconditions (void)
{
	char *extension = NULL;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*file_name != NULL*");
	g_assert_null (g_mime_types_get_mime_type (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*mime_type != NULL*");
	g_assert_false (g_mime_types_try_get_extension (NULL, &extension));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*extension != NULL*");
	g_assert_false (g_mime_types_try_get_extension ("text/plain", NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*mime_type != NULL*");
	g_mime_types_register (NULL, ".txt");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*mime_type != NULL*");
	g_mime_types_register ("", ".txt");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*extension != NULL*");
	g_mime_types_register ("text/plain", NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*extension != NULL*");
	g_mime_types_register ("text/plain", "");
	g_test_assert_expected_messages ();
	assert_mime_type ("file.txt", "text/plain");
	assert_extension ("text/plain", ".txt");
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME-type and file extension mappings");
	test_concurrency ();
	test_builtin_mappings ();
	test_paths ();
	test_registration ();
	test_preconditions ();
	g_mime_shutdown ();
	assert_mime_type ("file.gmime-one", "application/x-gmime-one");
	assert_extension ("application/x-gmime-three", ".GMime-Two");
	testsuite_end ();
	return testsuite_exit ();
}
