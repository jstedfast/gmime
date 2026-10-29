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
#include <gmime/gmime-object-private.h>
#endif

#include <string.h>

#include <gmime/gmime.h>
#include "testsuite.h"

static void
feed (GMimeFilter *filter, const char *data, size_t length, gboolean complete)
{
	char *output = NULL;
	size_t output_length, prespace;

	if (complete)
		g_mime_filter_complete (filter, (char *) data, length, 0, &output, &output_length, &prespace);
	else
		g_mime_filter_filter (filter, (char *) data, length, 0, &output, &output_length, &prespace);

	g_assert_true (output == data);
	g_assert_cmpuint (output_length, ==, length);
	g_assert_cmpuint (prespace, ==, 0);
}

static void
check_encodings (GMimeFilterBest *best, guint max_line_length, GMimeContentEncoding seven,
		 GMimeContentEncoding eight, GMimeContentEncoding binary)
{
	GMimeContentEncoding expected[] = { seven, eight, binary };
	guint i;

	for (i = 0; i < G_N_ELEMENTS (expected); i++) {
		g_assert_cmpint (g_mime_filter_best_encoding_with_max_line_length (best, i, max_line_length), ==, expected[i]);
		if (max_line_length == 78)
			g_assert_cmpint (g_mime_filter_best_encoding (best, i), ==, expected[i]);
	}
}

static void
check_splits (const char *data, size_t length, guint max_line_length, GMimeContentEncoding seven,
	      GMimeContentEncoding eight, GMimeContentEncoding binary)
{
	GMimeFilter *filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	size_t split;

	for (split = 0; split <= length; split++) {
		g_mime_filter_reset (filter);
		feed (filter, data, split, FALSE);
		feed (filter, data + split, length - split, TRUE);
		check_encodings (GMIME_FILTER_BEST (filter), max_line_length, seven, eight, binary);
	}
	g_object_unref (filter);
}

static void
test_markers_and_bytes (void)
{
	static const char *plain[] = {
		"", "\n", "\r\n", "\n\n", "Hello, world!", "From ", "From x",
		"From\t\n", "Fromx\n", "xFrom \n", "x\rFrom \n", "From\n",
	};
	static const char *markers[] = {
		"From \n", "From x\r\n", "x\nFrom \n", "\r\nFrom x\n",
		"\n\nFrom \r\n", "From x\nFrom y\n",
	};
	char density[100];
	guint i;

	for (i = 0; i < G_N_ELEMENTS (plain); i++)
		check_splits (plain[i], strlen (plain[i]), 78, GMIME_CONTENT_ENCODING_7BIT,
			      GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_7BIT);
	for (i = 0; i < G_N_ELEMENTS (markers); i++)
		check_splits (markers[i], strlen (markers[i]), 78, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			      GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);

	check_splits ("a\0b", 3, 78, GMIME_CONTENT_ENCODING_BASE64,
		      GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_BINARY);
	check_splits ("From \0\n", 7, 78, GMIME_CONTENT_ENCODING_BASE64,
		      GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_BASE64);
	check_splits ("\200", 1, 78, GMIME_CONTENT_ENCODING_BASE64,
		      GMIME_CONTENT_ENCODING_8BIT, GMIME_CONTENT_ENCODING_8BIT);

	for (i = 16; i <= 18; i++) {
		memset (density, 'a', sizeof (density));
		memset (density, 0x80, i);
		check_splits (density, sizeof (density), 78,
			      i >= 17 ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			      GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			      i > 17 ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
		check_splits (density, sizeof (density), 998,
			      i >= 17 ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			      GMIME_CONTENT_ENCODING_8BIT, GMIME_CONTENT_ENCODING_8BIT);
	}
}

static void
test_line_lengths (void)
{
	static const guint limits[] = { 60, 78, 998 };
	static const guint lengths[] = { 59, 60, 61, 77, 78, 79, 997, 998, 999 };
	GMimeContentEncoding expected;
	GMimeFilter *filter;
	char line[1002];
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (limits); i++) {
		for (j = 0; j < G_N_ELEMENTS (lengths); j++) {
			expected = lengths[j] > limits[i] ? GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE : GMIME_CONTENT_ENCODING_7BIT;
			memset (line, 'x', lengths[j]);
			check_splits (line, lengths[j], limits[i], expected, expected, expected);
			line[lengths[j]] = '\r';
			line[lengths[j] + 1] = '\n';
			check_splits (line, lengths[j] + 2, limits[i], expected, expected, expected);
		}
	}

	filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	memset (line, 'x', 78);
	line[78] = '\r';
	line[79] = '\n';

	/* The CR must not count toward the line length when LF arrives in the next buffer. */
	feed (filter, line, 79, TRUE);
	check_encodings (GMIME_FILTER_BEST (filter), 78, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	feed (filter, line + 79, 1, TRUE);
	check_encodings (GMIME_FILTER_BEST (filter), 78, GMIME_CONTENT_ENCODING_7BIT,
			 GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_7BIT);

	g_mime_filter_reset (filter);

	/* An unterminated final line still counts toward the maximum line length. */
	memset (line, 'x', 79);
	feed (filter, line, 79, FALSE);
	check_encodings (GMIME_FILTER_BEST (filter), 78, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	g_object_unref (filter);
}

static void
test_reset_copy_and_charset (void)
{
	GMimeFilterBestPrivate *copy_priv;
	GMimeFilterBestPrivate *filter_priv;
	GMimeFilter *filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING | GMIME_FILTER_BEST_CHARSET);
	GMimeFilter *charset = g_mime_filter_best_new (GMIME_FILTER_BEST_CHARSET);
	GMimeFilter *copy;

	feed (filter, "\303\251\nFrom \n", 9, FALSE);
	feed (charset, "\303\251\nFrom \n", 9, FALSE);
	g_assert_cmpstr (g_mime_filter_best_charset (GMIME_FILTER_BEST (filter)), ==,
			 g_mime_filter_best_charset (GMIME_FILTER_BEST (charset)));
	g_assert_cmpstr (g_mime_filter_best_charset (GMIME_FILTER_BEST (filter)), !=, "us-ascii");
	g_assert_cmpint (g_mime_filter_best_encoding (GMIME_FILTER_BEST (charset), GMIME_ENCODING_CONSTRAINT_7BIT), ==,
			 GMIME_CONTENT_ENCODING_UNSPECIFIED);
	check_encodings (GMIME_FILTER_BEST (filter), 78, GMIME_CONTENT_ENCODING_BASE64,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_BASE64);

	copy = g_mime_filter_copy (filter);
	copy_priv = _g_mime_filter_best_get_private (GMIME_FILTER_BEST (copy));

	filter_priv = _g_mime_filter_best_get_private (GMIME_FILTER_BEST (filter));

	g_assert_cmpint (copy_priv->flags, ==, filter_priv->flags);
	check_encodings (GMIME_FILTER_BEST (copy), 78, GMIME_CONTENT_ENCODING_7BIT,
			 GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_7BIT);
	g_assert_cmpstr (g_mime_filter_best_charset (GMIME_FILTER_BEST (copy)), ==, "us-ascii");

	g_mime_filter_reset (filter);
	feed (filter, "plain\r\n", 7, TRUE);
	check_encodings (GMIME_FILTER_BEST (filter), 78, GMIME_CONTENT_ENCODING_7BIT,
			 GMIME_CONTENT_ENCODING_7BIT, GMIME_CONTENT_ENCODING_7BIT);
	g_assert_cmpstr (g_mime_filter_best_charset (GMIME_FILTER_BEST (filter)), ==, "us-ascii");

	g_object_unref (copy);
	g_object_unref (charset);
	g_object_unref (filter);
}

static void
test_large_counters (void)
{
	GMimeFilter *filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	GMimeFilterBest *best = GMIME_FILTER_BEST (filter);
	GMimeFilterBestPrivate *priv = _g_mime_filter_best_get_private (best);

	priv->total = G_MAXUINT32;
	priv->count8 = G_MAXUINT32 / 2;
	feed (filter, "\200\200\0\n", 4, TRUE);
	g_assert_true (priv->total == (guint64) G_MAXUINT32 + 4);
	g_assert_true (priv->count8 == (guint64) G_MAXUINT32 / 2 + 2);
	g_assert_true (priv->count0 == 1);
	check_encodings (best, 998, GMIME_CONTENT_ENCODING_BASE64,
			 GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_BINARY);

	g_mime_filter_reset (filter);
	priv->total = G_MAXUINT32;
	priv->linelen = G_MAXUINT32;
	priv->previous = 'x';
	feed (filter, "x\r\n", 3, TRUE);
	g_assert_true (priv->maxline == (guint64) G_MAXUINT32 + 1);
	check_encodings (best, 998, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);

	g_mime_filter_reset (filter);
	priv->total = G_MAXUINT64 - 100;
	priv->count8 = (priv->total / 100) * 17 + ((priv->total % 100) * 17) / 100;
	priv->maxline = 1000;
	check_encodings (best, 998, GMIME_CONTENT_ENCODING_BASE64,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	priv->count8++;
	g_assert_cmpint (g_mime_filter_best_encoding (best, GMIME_ENCODING_CONSTRAINT_BINARY), ==, GMIME_CONTENT_ENCODING_BASE64);
	g_object_unref (filter);
}

static void
test_part_integration (void)
{
	GMimePart *part = g_mime_part_new_with_type ("text", "plain");
	GMimeStream *stream;
	GMimeContent *content;
	GMimeContentType *content_type;
	char *encoded;
	char data[79];

	g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, NULL), ==,
			 GMIME_CONTENT_ENCODING_7BIT);
	memset (data, 'x', sizeof (data));
	encoded = g_base64_encode ((const guchar *) data, sizeof (data));
	stream = g_mime_stream_mem_new_with_buffer (encoded, strlen (encoded));
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_BASE64);
	g_mime_part_set_content (part, content);
	g_object_unref (content);
	g_object_unref (stream);
	g_free (encoded);

	g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, NULL), ==,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	content_type = g_mime_content_type_new ("message", "rfc822");
	g_mime_entity_set_content_type (GMIME_ENTITY (part), content_type);
	g_object_unref (content_type);
	g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_BINARY, NULL), ==,
			 GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	content_type = g_mime_content_type_new ("application", "octet-stream");
	g_mime_entity_set_content_type (GMIME_ENTITY (part), content_type);
	g_object_unref (content_type);
	g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, NULL), ==,
			 GMIME_CONTENT_ENCODING_BASE64);
	g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_BINARY, NULL), ==,
			 GMIME_CONTENT_ENCODING_BINARY);
	g_object_unref (part);
}

static void
test_part_line_limits (void)
{
	static const guint limits[] = { 60, 78, 79, 998 };
	static const char *types[] = { "text", "message" };
	GMimePart *part;
	GMimeContent *content;
	GMimeStream *stream, *input;
	GMimeContentEncoding expected;
	GError *error = NULL;
	char data[79], *encoded, *raw;
	guint type, onebyte, limit, constraint;

	memset (data, 'x', sizeof (data));
	encoded = g_base64_encode ((const guchar *) data, sizeof (data));
	for (type = 0; type < G_N_ELEMENTS (types); type++) {
		for (onebyte = 0; onebyte < 2; onebyte++) {
			part = g_mime_part_new_with_type (types[type], type ? "global" : "plain");
			stream = g_mime_stream_mem_new_with_buffer (encoded, strlen (encoded));
			input = onebyte ? test_stream_onebyte_new (stream) : g_object_ref (stream);
			content = g_mime_content_new_with_stream (input, GMIME_CONTENT_ENCODING_BASE64);
			g_mime_part_set_content (part, content);
			g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
			raw = g_strdup (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Transfer-Encoding"));

			for (limit = 0; limit < G_N_ELEMENTS (limits); limit++) {
				expected = limits[limit] < sizeof (data) ?
					GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE : GMIME_CONTENT_ENCODING_7BIT;
				for (constraint = 0; constraint < 3; constraint++) {
					g_assert_cmpint (g_mime_part_get_best_content_encoding_with_max_line_length (
						part, constraint, limits[limit], &error), ==, expected);
					g_assert_no_error (error);
					g_assert_cmpint (g_mime_stream_tell (input), ==, 0);
					g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
				}
			}

			g_assert_cmpint (g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT,
									     &error), ==, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
			g_assert_no_error (error);
			g_assert_true (g_mime_part_get_content (part) == content);
			g_assert_cmpint (g_mime_content_get_encoding (content), ==, GMIME_CONTENT_ENCODING_BASE64);
			g_assert_cmpint (g_mime_part_get_content_encoding (part), ==, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
			g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (part), "Content-Transfer-Encoding"), ==, raw);
			g_free (raw);
			g_object_unref (content);
			g_object_unref (input);
			g_object_unref (stream);
			g_object_unref (part);
		}
	}
	g_free (encoded);
}

static void
test_preconditions (void)
{
	GMimeFilter *filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	GMimeFilterBest *best = GMIME_FILTER_BEST (filter);

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_FILTER_BEST*");
	g_assert_cmpint (g_mime_filter_best_encoding (NULL, GMIME_ENCODING_CONSTRAINT_7BIT), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*constraint >=*");
	g_assert_cmpint (g_mime_filter_best_encoding (best, -1), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*constraint >=*");
	g_assert_cmpint (g_mime_filter_best_encoding (best, 3), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_line_length >=*");
	g_assert_cmpint (g_mime_filter_best_encoding_with_max_line_length (best, GMIME_ENCODING_CONSTRAINT_7BIT, 59), ==,
			 GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*max_line_length >=*");
	g_assert_cmpint (g_mime_filter_best_encoding_with_max_line_length (best, GMIME_ENCODING_CONSTRAINT_7BIT, 999), ==,
			 GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_object_unref (filter);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MimeKit best-transfer-encoding analysis");
	test_markers_and_bytes ();
	test_line_lengths ();
	test_reset_copy_and_charset ();
	test_large_counters ();
	test_part_integration ();
	test_part_line_limits ();
	test_preconditions ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
