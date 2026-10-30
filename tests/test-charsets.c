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
test_large_names (void)
{
	const gsize length = 16 * 1024 * 1024;
	char *name = g_strnfill (length, 'x');
	const char *converter;

	g_mime_charset_map_init ();
	converter = g_mime_charset_iconv_name (name);
	g_assert_cmpuint (strlen (converter), ==, length);
	g_assert_cmpint (memcmp (converter, name, length), ==, 0);
	name[0] = 'X';
	g_assert_true (g_mime_charset_iconv_name (name) == converter);
	g_free (name);
	g_assert_cmpint (converter[0], ==, 'x');
	g_assert_cmpint (converter[length - 1], ==, 'x');
	g_mime_charset_map_shutdown ();
}

static void
test_iso_number_bounds (void)
{
	static const char *invalid[] = {
		"iso-4294976155-1", "iso-8859-4294967297",
		"iso-18446744073709551616-1", "iso-8859-18446744073709551616",
		"iso--18446744073709542757-1", "iso-8859--18446744073709551615"
	};
	char *limit = g_strdup_printf ("%u", G_MAXUINT);
	char *name = g_strdup_printf ("iso-%s-%s", limit, limit);
	char *overflow = g_strdup_printf ("iso-%" G_GUINT64_FORMAT "-1", (guint64) G_MAXUINT + 1);
	const char *converter;
	guint i;

	g_mime_charset_map_init ();
	for (i = 0; i < G_N_ELEMENTS (invalid); i++)
		g_assert_cmpstr (g_mime_charset_iconv_name (invalid[i]), ==, invalid[i]);
	g_assert_cmpstr (g_mime_charset_iconv_name (overflow), ==, overflow);
	converter = g_mime_charset_iconv_name (name);
	g_assert_nonnull (strstr (converter, limit));
	g_assert_true (g_str_has_suffix (converter, limit));
	g_assert_cmpstr (g_mime_charset_iconv_name ("ISO_8859_01"), ==,
			g_mime_charset_iconv_name ("iso-8859-1"));
	g_assert_cmpstr (g_mime_charset_iconv_name ("ISO_2022_JP"), ==,
			g_mime_charset_iconv_name ("iso-2022-jp"));
	g_mime_charset_map_shutdown ();
	g_free (overflow);
	g_free (name);
	g_free (limit);
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
		decoded = _g_mime_iconv_convert (cases[i].bytes, cases[i].length, "UTF-8", converter, NULL, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (decoded, ==, "caf\303\251");
		g_free (decoded);
	}
	g_assert_cmpstr (g_mime_charset_iconv_name ("UTF-16"), ==, "UTF-16");
	g_assert_cmpstr (g_mime_charset_iconv_name ("UTF-32"), ==, "UTF-32");
	g_mime_charset_map_shutdown ();
}

static void
test_native_aliases (void)
{
	static const struct {
		const char *alias, *converter, *bytes, *text, *language;
	} cases[] = {
		{ "UJIS", "euc-jp", "\244\242", "\343\201\202", "ja" },
		{ "ujis-0", "euc-jp", "\244\242", "\343\201\202", "ja" },
		{ "eucjp-0", "euc-jp", "\244\242", "\343\201\202", "ja" },
		{ "BIG5HKSCS", "BIG5-HKSCS", "\234\161", "\360\240\200\241", "zh" },
		{ "big5hkscs-0", "BIG5-HKSCS", "\234\161", "\360\240\200\241", "zh" }
	};
	guint i;

	g_mime_charset_map_init ();
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		const char *converter = g_mime_charset_iconv_name (cases[i].alias);
		GError *error = NULL;
		char *decoded;

		g_assert_cmpstr (converter, ==, cases[i].converter);
		g_assert_cmpstr (g_mime_charset_language (converter), ==, cases[i].language);
		decoded = _g_mime_iconv_convert (cases[i].bytes, -1, "UTF-8", converter, NULL, &error);
		g_assert_no_error (error);
		g_assert_cmpstr (decoded, ==, cases[i].text);
		g_free (decoded);
	}
	g_mime_charset_map_shutdown ();
}

static void
test_checked_conversion (void)
{
	static const struct {
		const char *text, *to;
		GConvertError code;
	} invalid[] = {
		{ "prefix\377", "UTF-8", G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "prefix\342\202", "UTF-8", G_CONVERT_ERROR_PARTIAL_INPUT },
		{ "\360\237\230\200", "US-ASCII", G_CONVERT_ERROR_ILLEGAL_SEQUENCE },
		{ "", "x-gmime-unsupported", G_CONVERT_ERROR_NO_CONVERSION },
		{ "", "x-unknown", G_CONVERT_ERROR_NO_CONVERSION }
	};
	GString *text = g_string_new (NULL), *expected = g_string_new (NULL);
	GError *error = NULL;
	char *converted, *decoded;
	gsize length, decoded_length;
	iconv_t cd;
	guint i;

	g_mime_charset_map_init ();
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		converted = _g_mime_iconv_convert (invalid[i].text, -1, invalid[i].to, "UTF-8", &length, &error);
		g_assert_null (converted);
		g_assert_error (error, G_CONVERT_ERROR, invalid[i].code);
		g_assert_cmpuint (length, ==, 0);
		g_clear_error (&error);
	}

	converted = _g_mime_iconv_convert ("", 0, "UTF-8", "UTF-8", &length, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (length, ==, 0);
	g_assert_cmpmem (converted, 4, "\0\0\0\0", 4);
	g_free (converted);
	converted = _g_mime_iconv_convert ("A\0B", 3, "UTF-32LE", "UTF-8", &length, &error);
	g_assert_no_error (error);
	g_assert_cmpmem (converted, length, "A\0\0\0\0\0\0\0B\0\0\0", 12);
	g_assert_cmpmem (converted + length, 4, "\0\0\0\0", 4);
	decoded = _g_mime_iconv_convert (converted, length, "UTF-8", "UTF-32LE", &decoded_length, &error);
	g_assert_no_error (error);
	g_assert_cmpmem (decoded, decoded_length, "A\0B", 3);
	g_free (decoded);
	g_free (converted);

	for (i = 0; i < 512; i++) {
		g_string_append (text, "\346\227\245ABC");
		g_string_append (expected, "\033$BF|\033(BABC");
	}
	converted = _g_mime_iconv_convert (text->str, text->len, "ISO-2022-JP", "UTF-8", &length, &error);
	g_assert_no_error (error);
	g_assert_cmpmem (converted, length, expected->str, expected->len);
	decoded = _g_mime_iconv_convert (converted, length, "UTF-8", "ISO-2022-JP", &decoded_length, &error);
	g_assert_no_error (error);
	g_assert_cmpmem (decoded, decoded_length, text->str, text->len);
	g_free (converted);
	g_free (decoded);
	g_string_free (expected, TRUE);
	g_string_free (text, TRUE);

	/* Exactly fill the first output buffer, leaving the reset sequence to grow it. */
	text = g_string_new (NULL);
	for (i = 0; i < 55; i++)
		g_string_append_c (text, 'a');
	g_string_append (text, "\346\227\245\346\227\245\346\227\245");
	converted = _g_mime_iconv_convert (text->str, text->len, "ISO-2022-JP", "UTF-8", &length, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (length, ==, 67);
	g_assert_cmpmem (converted, 55, text->str, 55);
	g_assert_cmpmem (converted + 55, 12, "\033$BF|F|F|\033(B", 12);
	g_free (converted);
	g_string_free (text, TRUE);

	cd = g_mime_iconv_open ("ISO-2022-JP", "UTF-8");
	g_assert_true (cd != (iconv_t) -1);
	for (i = 0; i < 2; i++) {
		converted = _g_mime_iconv_convert_with_iconv ("\346\227\245", 3, cd, &length, &error);
		g_assert_no_error (error);
		g_assert_cmpmem (converted, length, "\033$BF|\033(B", 8);
		g_free (converted);
	}
	converted = _g_mime_iconv_convert_with_iconv ("", G_MAXSIZE, cd, &length, &error);
	g_assert_null (converted);
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_MEMORY);
	g_clear_error (&error);
	g_mime_iconv_close (cd);
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
	g_test_add_func ("/charsets/large-names", test_large_names);
	g_test_add_func ("/charsets/iso-number-bounds", test_iso_number_bounds);
	g_test_add_func ("/charsets/text-converters", test_text_converters);
	g_test_add_func ("/charsets/native-aliases", test_native_aliases);
	g_test_add_func ("/charsets/checked-conversion", test_checked_conversion);
	g_test_add_func ("/charsets/native-identities", test_native_identities);
	g_test_add_func ("/charsets/lifetime", test_lifetime);
	g_test_add_func ("/charsets/threads", test_threads);
	return g_test_run ();
}
