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
		{ ".aiff", "audio/x-aiff" },
		{ ".apng", "image/apng" },
		{ ".arrow", "application/vnd.apache.arrow.file" },
		{ ".avi", "video/x-msvideo" },
		{ ".avif", "image/avif" },
		{ ".cbor", "application/cbor" },
		{ ".class", "application/java-vm" },
		{ ".dart", "application/vnd.dart" },
		{ ".deb", "application/vnd.debian.binary-package" },
		{ ".dwg", "image/vnd.dwg" },
		{ ".dxf", "image/vnd.dxf" },
		{ ".eot", "application/vnd.ms-fontobject" },
		{ ".flac", "audio/flac" },
		{ ".geojson", "application/geo+json" },
		{ ".glb", "model/gltf-binary" },
		{ ".gltf", "model/gltf+json" },
		{ ".gz", "application/gzip" },
		{ ".heic", "image/heic" },
		{ ".heics", "image/heic-sequence" },
		{ ".heif", "image/heif" },
		{ ".heifs", "image/heif-sequence" },
		{ ".hqx", "application/mac-binhex40" },
		{ ".jp2", "image/jp2" },
		{ ".jpf", "image/jpx" },
		{ ".jsonld", "application/ld+json" },
		{ ".jxl", "image/jxl" },
		{ ".m3u", "audio/x-mpegurl" },
		{ ".m3u8", "application/vnd.apple.mpegurl" },
		{ ".mif", "application/vnd.mif" },
		{ ".mka", "audio/matroska" },
		{ ".mkv", "video/matroska" },
		{ ".obj", "model/obj" },
		{ ".odf", "application/vnd.oasis.opendocument.formula" },
		{ ".odg", "application/vnd.oasis.opendocument.graphics" },
		{ ".otg", "application/vnd.oasis.opendocument.graphics-template" },
		{ ".otp", "application/vnd.oasis.opendocument.presentation-template" },
		{ ".ots", "application/vnd.oasis.opendocument.spreadsheet-template" },
		{ ".ott", "application/vnd.oasis.opendocument.text-template" },
		{ ".p12", "application/pkcs12" },
		{ ".parquet", "application/vnd.apache.parquet" },
		{ ".pcl", "application/vnd.hp-PCL" },
		{ ".pnm", "image/x-portable-anymap" },
		{ ".psd", "image/vnd.adobe.photoshop" },
		{ ".py", "text/x-script.python" },
		{ ".roff", "text/troff" },
		{ ".smil", "application/smil+xml" },
		{ ".sqlite3", "application/vnd.sqlite3" },
		{ ".step", "model/step" },
		{ ".stl", "model/stl" },
		{ ".toml", "application/toml" },
		{ ".vsd", "application/vnd.visio" },
		{ ".vtt", "text/vtt" },
		{ ".wasm", "application/wasm" },
		{ ".webmanifest", "application/manifest+json" },
		{ ".wpd", "application/vnd.wordperfect" },
		{ ".wrl", "model/vrml" },
		{ ".xls", "application/vnd.ms-excel" },
		{ ".xpm", "image/x-xpixmap" },
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
	assert_extension ("image/jpeg", ".jpg");
	assert_extension ("text/html", ".html");
	assert_extension ("application/x-gmime-unknown", NULL);
	assert_extension ("", NULL);
	assert_extension (" text/plain", NULL);
	assert_extension ("text/plain ", NULL);
	assert_extension ("text/plain; charset=utf-8", NULL);
}

static void
test_mapping_aliases (void)
{
	static const struct {
		const char *extension;
		const char *mime_type;
		const char *preferred_extension;
	} alternate_extensions[] = {
		{ ".yml", "application/yaml", ".yaml" },
		{ ".gzip", "application/gzip", ".gz" },
		{ ".aif", "audio/x-aiff", ".aiff" },
		{ ".aifc", "audio/x-aiff", ".aiff" },
		{ ".asx", "video/x-ms-asf", ".asf" },
		{ ".au", "audio/basic", ".snd" },
		{ ".jpg2", "image/jp2", ".jp2" },
		{ ".jpx", "image/jpx", ".jpf" },
		{ ".man", "text/troff", ".roff" },
		{ ".me", "text/troff", ".roff" },
		{ ".ms", "text/troff", ".roff" },
		{ ".opus", "audio/ogg", ".ogg" },
		{ ".pfx", "application/pkcs12", ".p12" },
		{ ".pm", "image/x-xpixmap", ".xpm" },
		{ ".smi", "application/smil+xml", ".smil" },
		{ ".sml", "application/smil+xml", ".smil" },
		{ ".sqlite", "application/vnd.sqlite3", ".sqlite3" },
		{ ".stp", "model/step", ".step" },
		{ ".t", "text/troff", ".roff" },
		{ ".tr", "text/troff", ".roff" },
		{ ".udeb", "application/vnd.debian.binary-package", ".deb" },
		{ ".vrml", "model/vrml", ".wrl" },
		{ ".vss", "application/vnd.visio", ".vsd" },
		{ ".vst", "application/vnd.visio", ".vsd" },
		{ ".vsw", "application/vnd.visio", ".vsd" },
		{ ".w60", "application/vnd.wordperfect", ".wpd" },
		{ ".w61", "application/vnd.wordperfect", ".wpd" },
		{ ".wp", "application/vnd.wordperfect", ".wpd" },
		{ ".wp5", "application/vnd.wordperfect", ".wpd" },
		{ ".wp6", "application/vnd.wordperfect", ".wpd" },
		{ ".wrz", "model/vrml", ".wrl" },
		{ ".wsc", "text/scriptlet", ".sct" },
		{ ".xl", "application/vnd.ms-excel", ".xls" },
		{ ".xlb", "application/vnd.ms-excel", ".xls" },
		{ ".xlc", "application/vnd.ms-excel", ".xls" },
		{ ".xld", "application/vnd.ms-excel", ".xls" },
		{ ".xlk", "application/vnd.ms-excel", ".xls" },
		{ ".xll", "application/vnd.ms-excel", ".xls" },
		{ ".xlm", "application/vnd.ms-excel", ".xls" },
		{ ".xlv", "application/vnd.ms-excel", ".xls" },
		{ ".xlw", "application/vnd.ms-excel", ".xls" },
	};
	static const struct {
		const char *mime_type;
		const char *extension;
	} legacy_types[] = {
		{ "application/x-gzip", ".gz" },
		{ "audio/x-flac", ".flac" },
		{ "video/x-matroska", ".mkv" },
		{ "application/sla", ".stl" },
		{ "application/acad", ".dwg" },
		{ "application/binhex", ".hqx" },
		{ "application/dart", ".dart" },
		{ "application/dxf", ".dxf" },
		{ "application/excel", ".xls" },
		{ "application/pkcs-12", ".p12" },
		{ "application/smil", ".smi" },
		{ "application/step", ".step" },
		{ "application/vnd.bw-fontobject", ".eot" },
		{ "application/wordperfect", ".wp5" },
		{ "application/wordperfect6.0", ".w60" },
		{ "application/wordperfect6.1", ".w61" },
		{ "application/x-deb", ".deb" },
		{ "application/x-debian-package", ".deb" },
		{ "application/x-java-applet", ".class" },
		{ "application/x-mif", ".mif" },
		{ "application/x-mplayer2", ".asx" },
		{ "application/x-pcl", ".pcl" },
		{ "application/x-pkcs12", ".pfx" },
		{ "application/x-portable-anymap", ".pnm" },
		{ "application/x-sqlite3", ".sqlite3" },
		{ "application/x-troff", ".roff" },
		{ "application/x-troff-man", ".man" },
		{ "application/x-troff-me", ".me" },
		{ "application/x-troff-ms", ".ms" },
		{ "application/x-visio", ".vsd" },
		{ "application/x-vrml", ".vrml" },
		{ "application/x-world", ".svr" },
		{ "audio/aiff", ".aiff" },
		{ "audio/opus", ".opus" },
		{ "audio/x-au", ".au" },
		{ "audio/x-matroska", ".mka" },
		{ "audio/x-mpequrl", ".m3u" },
		{ "image/xpm", ".xpm" },
		{ "text/scriplet", ".wsc" },
		{ "text/x-script.phyton", ".py" },
		{ "video/avi", ".avi" },
	};
	static const struct {
		const char *file_name;
		const char *mime_type;
	} preserved[] = {
		{ "filename.ts", "application/typescript" },
		{ "filename.rpm", "audio/x-pn-realaudio-plugin" },
		{ "filename.xml", "text/xml" },
		{ "filename.rtf", "text/rtf" },
		{ "filename.db", "application/octet-stream" },
	};
	char *file_name, *upper;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (alternate_extensions); i++) {
		file_name = g_strconcat ("filename", alternate_extensions[i].extension, NULL);
		assert_mime_type (file_name, alternate_extensions[i].mime_type);
		upper = g_ascii_strup (file_name, -1);
		assert_mime_type (upper, alternate_extensions[i].mime_type);
		assert_extension (alternate_extensions[i].mime_type, alternate_extensions[i].preferred_extension);
		g_free (upper);
		g_free (file_name);
	}
	for (i = 0; i < G_N_ELEMENTS (legacy_types); i++) {
		assert_extension (legacy_types[i].mime_type, legacy_types[i].extension);
		upper = g_ascii_strup (legacy_types[i].mime_type, -1);
		assert_extension (upper, legacy_types[i].extension);
		g_free (upper);
	}
	for (i = 0; i < G_N_ELEMENTS (preserved); i++)
		assert_mime_type (preserved[i].file_name, preserved[i].mime_type);
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
	test_mapping_aliases ();
	test_paths ();
	test_registration ();
	test_preconditions ();
	g_mime_shutdown ();
	assert_mime_type ("file.gmime-one", "application/x-gmime-one");
	assert_extension ("application/x-gmime-three", ".GMime-Two");
	testsuite_end ();
	return testsuite_exit ();
}
