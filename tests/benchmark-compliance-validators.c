/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2026 Jeffrey Stedfast
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include <gmime/gmime-encoded-word-validator-private.h>
#include <gmime/gmime-parameter-validator-private.h>

static void
count_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
	guint64 *count = user_data;

	(*count)++;
}

static void
run (const char *name, const char *text, guint repeat, gboolean parameters,
     gboolean content_type, GMimeEncodedWordContext context)
{
	GString *input = g_string_new (NULL);
	guint64 count = 0;
	gint64 start, elapsed;
	guint i, iterations;

	for (i = 0; i < repeat; i++)
		g_string_append (input, text);

	iterations = MAX (2000, 20000000 / input->len);
	start = g_get_monotonic_time ();
	for (i = 0; i < iterations; i++) {
		if (parameters) {
			_g_mime_parameter_validate ((const guint8 *) input->str, input->len, content_type,
				count_issue, &count, GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
		} else {
			GMimeEncodedWordValidator words;

			_g_mime_encoded_word_validator_init (&words, (const guint8 *) input->str, input->len,
				count_issue, &count, GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
			_g_mime_encoded_word_validator_validate (&words, input->len, context);
		}
	}
	elapsed = g_get_monotonic_time () - start;
	printf ("%-24s %10.1f ns/header %8" G_GUINT64_FORMAT " issues\n",
		name, elapsed * 1000.0 / iterations, count / iterations);
	g_string_free (input, TRUE);
}

int
main (void)
{
	static const struct {
		const char *name, *text;
		guint repeat;
		GMimeEncodedWordContext context;
	} words[] = {
		{ "PlainSubject", " Monthly report\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "LongPlainSubject", " Ordinary folded header text without encoded words.\r\n", 80, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "ShortQ", " =?utf-8?Q?caf=C3=A9?=\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "ShortB", " =?utf-8?B?Y2Fmw6k=?=\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "FoldedQ", " =?utf-8?Q?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=20?=\r\n", 64, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "FoldedB", " =?utf-8?B?AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA?=\r\n", 64, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "PlainDisplayName", " Alice Example <alice@example.com>\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_PHRASE },
		{ "Comments", " (outer (=?utf-8?Q?caf=C3=A9?=)) Alice <alice@example.com>\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_PHRASE },
		{ "QuotedWord", " \"=?utf-8?Q?Alice_Example?=\" <alice@example.com>\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_PHRASE },
		{ "MalformedQ", " =?utf-8?Q?bad=GG?=\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "MalformedB", " =?utf-8?B?YQ?=\r\n", 1, GMIME_ENCODED_WORD_CONTEXT_TEXT },
		{ "Unterminated", "=?utf-8?Q?unfinished", 128, GMIME_ENCODED_WORD_CONTEXT_TEXT }
	};
	static const struct {
		const char *name, *text;
		gboolean content_type;
	} parameters[] = {
		{ "ParameterNone", " text/plain\r\n", TRUE },
		{ "ParameterCharset", " text/plain; charset=utf-8\r\n", TRUE },
		{ "ParameterFilename", " attachment; filename=\"report.pdf\"\r\n", FALSE },
		{ "ParameterPair", " text/plain; charset=utf-8; format=flowed\r\n", TRUE },
		{ "ParameterContinuation", " attachment; filename*0*=utf-8''monthly%20;\r\n filename*1*=report.pdf\r\n", FALSE },
		{ "ParameterRepeated", " multipart/mixed; boundary=one; boundary=two\r\n", TRUE }
	};
	GString *text = g_string_new (NULL);
	char *padding = g_strnfill (2048, 'a');
	guint i;

	for (i = 0; i < G_N_ELEMENTS (words); i++)
		run (words[i].name, words[i].text, words[i].repeat, FALSE, TRUE, words[i].context);

	for (i = 0; i < 20; i++) {
		if (i)
			g_string_append (text, ",\r\n ");
		g_string_append (text, "=?utf-8?Q?Alice_Example?= <alice@example.com>");
	}
	run ("EncodedDisplayNames", text->str, 1, FALSE, TRUE, GMIME_ENCODED_WORD_CONTEXT_PHRASE);
	g_string_printf (text, "%s\r\n =?utf-8?Q?valid?=\r\n %s\r\n =?utf-8?Q?bad=GG?=\r\n", padding, padding);
	run ("SparseWords", text->str, 1, FALSE, TRUE, GMIME_ENCODED_WORD_CONTEXT_TEXT);
	g_free (padding);

	for (i = 0; i < G_N_ELEMENTS (parameters); i++)
		run (parameters[i].name, parameters[i].text, 1, TRUE, parameters[i].content_type, GMIME_ENCODED_WORD_CONTEXT_TEXT);

	g_string_assign (text, " text/plain");
	for (i = 0; i < 100; i++)
		g_string_append_printf (text, "; x%u=value", i);
	g_string_append (text, "\r\n");
	run ("ParameterManyNames", text->str, 1, TRUE, TRUE, GMIME_ENCODED_WORD_CONTEXT_TEXT);
	g_string_assign (text, " attachment; filename*0*=utf-8''a");
	for (i = 1; i <= 100; i++)
		g_string_append_printf (text, "; filename*%u*=a", i);
	g_string_append (text, "\r\n");
	run ("ParameterManySegments", text->str, 1, TRUE, FALSE, GMIME_ENCODED_WORD_CONTEXT_TEXT);
	g_string_free (text, TRUE);
	return 0;
}
