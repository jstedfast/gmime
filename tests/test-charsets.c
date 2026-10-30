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

#include <gmime/gmime-charset.h>
#include <gmime/gmime-internal.h>

static void
test_codepages (void)
{
	static const struct {
		const char *name;
		int codepage;
	} cases[] = {
		{ "UTF8", 65001 }, { "utf-8", 65001 },
		{ "ANSI_X3.4-1968", 20127 }, { "ascii", 20127 }, { "us-ascii", 20127 },
		{ "latin1", 28591 }, { "iso_8859_1", 28591 }, { "ansi_x3.110-1983", 28591 },
		{ "latin2", 28592 }, { "latin5", 28599 }, { "latin9", 28605 },
		{ "windows-1251", 1251 }, { "cp1251", 1251 }, { "microsoft-cp1251", 1251 },
		{ "windows-cp1252", 1252 }, { "CP1252", 1252 },
		{ "euc-jp", 51932 }, { "eucJP", 51932 }, { "ujis", 51932 }, { "ujis-0", 51932 },
		{ "shift_jis", 932 }, { "sjis", 932 }, { "pck", 932 },
		{ "ks_c_5601-1987", 51949 }, { "euc-kr", 51949 },
		{ "gb2312", 936 }, { "gbk", 936 }, { "euc-cn", 936 },
		{ "GB2312", 936 }, { "gb-2312", 936 }, { "gb2312-0", 936 },
		{ "gb2312-80", 936 }, { "gb2312.1980-0", 936 },
		{ "big5.eten-0", 950 }, { "big5", 950 },
		{ "gb18030-0", 54936 }, { "gb18030", 54936 },
		{ "iso-2022-jp", 50220 }, { "iso-2022-kr", 50225 },
		{ "utf-16le", 1200 }, { "utf-16be", 1201 },
		{ "utf-32le", 12000 }, { "utf-32be", 12001 },
		{ "koi8-r", 20866 }, { "koi8-u", 21866 }
	};
	guint i, repeat;

	for (repeat = 0; repeat < 2; repeat++) {
		g_mime_charset_map_init ();
		for (i = 0; i < G_N_ELEMENTS (cases); i++) {
			char *name = g_strdup (cases[i].name);
			const char *converter = g_mime_charset_iconv_name (name);

			g_assert_cmpint (_g_mime_charset_get_codepage (name), ==, cases[i].codepage);
			g_free (name);
			g_assert_cmpint (_g_mime_charset_get_codepage (converter), ==, cases[i].codepage);
		}
		g_mime_charset_map_shutdown ();
	}
}

static void
test_text_converters (void)
{
	static const struct {
		const char *name, *converter, *bytes;
		gsize length;
	} cases[] = {
		{ "utf-16", "UTF-16LE", "c\0a\0f\0\351\0", 8 },
		{ "UTF-16", "UTF-16LE", "c\0a\0f\0\351\0", 8 },
		{ "utf-32", "UTF-32LE", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "UTF-32", "UTF-32LE", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf-16be", "utf-16be", "\0c\0a\0f\0\351", 8 },
		{ "utf-32be", "utf-32be", "\0\0\0c\0\0\0a\0\0\0f\0\0\0\351", 16 }
	};
	guint i;

	g_mime_charset_map_init ();
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		const char *converter = _g_mime_charset_get_text_converter (cases[i].name);
		GError *error = NULL;
		char *decoded;

		g_assert_cmpstr (converter, ==, cases[i].converter);
		decoded = g_convert (cases[i].bytes, cases[i].length, "UTF-8", converter, NULL, NULL, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (decoded, ==, "caf\303\251");
		g_free (decoded);
	}
	g_assert_cmpstr (g_mime_charset_iconv_name ("UTF-16"), ==, "UTF-16");
	g_assert_cmpstr (g_mime_charset_iconv_name ("UTF-32"), ==, "UTF-32");
	g_mime_charset_map_shutdown ();
}

static void
test_native_identities (void)
{
	GHashTable *seen = g_hash_table_new (g_direct_hash, g_direct_equal);
	int codepage, other;
	guint i, repeat;

	for (repeat = 0; repeat < 2; repeat++) {
		g_mime_charset_map_init ();
		g_hash_table_remove_all (seen);
		for (i = 0; i < 1024; i++) {
			char *name = g_strdup_printf ("x-native-%u", i);
			char *upper = g_ascii_strup (name, -1);

			codepage = _g_mime_charset_get_codepage (name);
			g_assert_cmpint (codepage, <, 0);
			g_assert_cmpint (_g_mime_charset_get_codepage (upper), ==, codepage);
			g_assert_true (g_hash_table_add (seen, GINT_TO_POINTER (codepage)));
			g_free (upper);
			g_free (name);
		}
		codepage = _g_mime_charset_get_codepage ("utf-16");
		other = _g_mime_charset_get_codepage ("utf-16le");
		g_assert_cmpint (codepage, !=, other);
		codepage = _g_mime_charset_get_codepage ("ucs-2be");
		other = _g_mime_charset_get_codepage ("utf-16be");
		g_assert_cmpint (codepage, !=, other);
		codepage = _g_mime_charset_get_codepage ("iso-8859-10");
		other = _g_mime_charset_get_codepage ("iso_8859_10");
		g_assert_cmpint (codepage, ==, other);
		g_mime_charset_map_shutdown ();
	}
	g_hash_table_destroy (seen);
}

static gpointer
lookup_thread (gpointer user_data)
{
	int *codepages = user_data;
	guint i, repeat;

	for (repeat = 0; repeat < 8; repeat++) {
		for (i = 0; i < 64; i++) {
			char *name = g_strdup_printf (repeat % 2 ? "X-THREADED-%u" : "x-threaded-%u", i);
			int codepage = _g_mime_charset_get_codepage (name);

			if (repeat == 0)
				codepages[i] = codepage;
			else
				g_assert_cmpint (codepage, ==, codepages[i]);
			g_free (name);
		}
	}
	return NULL;
}

static void
test_lifetime (void)
{
	int codepage;

	g_mime_charset_map_init ();
	codepage = _g_mime_charset_get_codepage ("x-retained");
	g_mime_charset_map_init ();
	g_assert_cmpint (_g_mime_charset_get_codepage ("X-RETAINED"), ==, codepage);
	g_mime_charset_map_shutdown ();
	g_assert_cmpint (_g_mime_charset_get_codepage ("x-retained"), ==, codepage);
	g_assert_cmpint (_g_mime_charset_get_codepage ("utf-8"), ==, 65001);
	g_mime_charset_map_shutdown ();

	g_mime_charset_map_init ();
	g_assert_cmpint (_g_mime_charset_get_codepage ("x-retained"), <, 0);
	g_assert_cmpint (_g_mime_charset_get_codepage ("utf-8"), ==, 65001);
	g_mime_charset_map_shutdown ();
}

static void
test_threads (void)
{
	int codepages[8][64];
	GThread *threads[8];
	guint i, j;

	g_mime_charset_map_init ();
	for (i = 0; i < G_N_ELEMENTS (threads); i++)
		threads[i] = g_thread_new ("charset-codepages", lookup_thread, codepages[i]);
	for (i = 0; i < G_N_ELEMENTS (threads); i++)
		g_thread_join (threads[i]);
	for (i = 1; i < G_N_ELEMENTS (threads); i++) {
		for (j = 0; j < G_N_ELEMENTS (codepages[i]); j++)
			g_assert_cmpint (codepages[i][j], ==, codepages[0][j]);
	}
	g_mime_charset_map_shutdown ();
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/charsets/codepages", test_codepages);
	g_test_add_func ("/charsets/text-converters", test_text_converters);
	g_test_add_func ("/charsets/native-identities", test_native_identities);
	g_test_add_func ("/charsets/lifetime", test_lifetime);
	g_test_add_func ("/charsets/threads", test_threads);
	return g_test_run ();
}
