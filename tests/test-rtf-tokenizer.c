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

#include <gmime/gmime.h>
#include "gmime/gmime-rtf-tokenizer-private.h"
#include <string.h>

static void
test_overlong_control_words (void)
{
	char *word = g_strnfill (1000, 'a'), *digits = g_strnfill (1000, '9');
	char *input = g_strconcat ("\\", word, "123 \\fs", digits, " \\fs-", digits, " x", NULL);
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
	GMimeRtfTokenizer *lexer = _g_mime_rtf_tokenizer_new (stream);
	GMimeRtfToken token;
	GError *error = NULL;

	for (int i = 0; i < 3; i++) {
		g_assert_true (_g_mime_rtf_tokenizer_next (lexer, &token, &error));
		g_assert_no_error (error);
		g_assert_cmpint (token.kind, ==, GMIME_RTF_CONTROL_WORD);
		g_assert_true (token.has_parameter);
		if (!i) {
			g_assert_true (token.keyword_truncated);
			g_assert_cmpstr (token.keyword, ==, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
			g_assert_cmpint (token.parameter, ==, 123);
		} else {
			g_assert_cmpstr (token.keyword, ==, "fs");
			g_assert_true (token.parameter_invalid);
			g_assert_cmpint (token.parameter, ==, i == 1 ? G_MAXINT32 : G_MININT32);
		}
	}
	g_assert_true (_g_mime_rtf_tokenizer_next (lexer, &token, &error));
	g_assert_cmpint (token.kind, ==, GMIME_RTF_TEXT);
	g_assert_cmpmem (token.text, token.length, "x", 1);
	for (int i = 0; i < 3; i++) {
		g_assert_true (_g_mime_rtf_tokenizer_next (lexer, &token, &error));
		g_assert_cmpint (token.kind, ==, GMIME_RTF_EOF);
	}
	g_assert_no_error (error);
	_g_mime_rtf_tokenizer_free (lexer);
	g_object_unref (stream);
	g_free (input);
	g_free (digits);
	g_free (word);
}

static void
test_tokens (void)
{
	/* Put the backslash in the last byte of a refill. Binary contains a
	 * brace and LF, neither of which should be interpreted as syntax. */
	char *spaces = g_strnfill (4095, ' ');
	char *input = g_strconcat (spaces, "\\bin3 }{\n{\\x-2147483648\\'f!}", NULL);
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (input, strlen (input));
	GMimeRtfTokenizer *lexer = _g_mime_rtf_tokenizer_new (stream);
	GMimeRtfToken token;
	GError *error = NULL;
	const GMimeRtfTokenKind expected[] = {
		GMIME_RTF_TEXT, GMIME_RTF_CONTROL_WORD, GMIME_RTF_GROUP_START, GMIME_RTF_CONTROL_WORD,
		GMIME_RTF_HEX_CHAR, GMIME_RTF_TEXT, GMIME_RTF_GROUP_END, GMIME_RTF_EOF
	};

	for (guint i = 0; i < G_N_ELEMENTS (expected); i++) {
		g_assert_true (_g_mime_rtf_tokenizer_next (lexer, &token, &error));
		g_assert_no_error (error);
		g_assert_cmpint (token.kind, ==, expected[i]);
		if (i == 2) {
			g_assert_cmpuint (token.depth, ==, 0);
			g_assert_cmpuint (token.line, ==, 2);
			g_assert_cmpuint (token.column, ==, 1);
		} else if (i == 3) {
			g_assert_true (token.has_parameter);
			g_assert_false (token.parameter_invalid);
			g_assert_cmpint (token.parameter, ==, G_MININT32);
		} else if (i == 4) {
			g_assert_cmpint (token.value, ==, 15);
		} else if (i == 5) {
			g_assert_cmpmem (token.text, token.length, "!", 1);
		}
	}
	g_assert_false (token.truncated);
	_g_mime_rtf_tokenizer_free (lexer);
	g_object_unref (stream);
	g_free (input);
	g_free (spaces);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/rtf/tokenizer/refill-and-binary", test_tokens);
	g_test_add_func ("/rtf/tokenizer/overlong-control-words", test_overlong_control_words);
	int result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
