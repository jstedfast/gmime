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

#include "benchmark.h"
#include <gmime/gmime-address-validator-private.h>
#include <gmime/gmime-reader-simd-private.h>
#include <string.h>

typedef struct {
	const char *name, *format;
	guint count;
} AddressInput;

static const AddressInput parser_inputs[] = {
	{ "Single", "Joe Sixpack <joe@example.com>", 1 },
	{ "Bare", "first.last@example.com", 1 },
	{ "Recipients", "User %u <u%u@example.com>", 100 },
	{ "Utf8", "Ren\303\251e M\303\274ller %u <u%u@example.com>", 100 },
	{ "Encoded", "=?utf-8?q?Ren=C3=A9e?= =?utf-8?q?_M=C3=BCller?= <u%u@example.com>", 100 },
	{ "MixedEncoded", NULL, 100 },
	{ "Groups", "Group %u: a%u@example.com, b%u@example.com;", 20 },
	{ "Broken", "<<<u%u@example.com>>>", 100 },
	{ "LongAtoms", NULL, 100 },
	{ "Quoted", "\"Last, First %u\" <u%u@example.com>", 100 },
	{ "Comment", NULL, 1 },
	{ "UnicodeAddr", "\303\251%u@\344\270\255.example", 100 },
	{ "QuotedLocal", "\"first last %u\"@example.com", 100 },
	{ "CFWSDomain", "a%u@example (c) . com", 100 },
	{ "TrailingDot", "a%u@example.com.", 100 },
	{ "Utf8Cjk", "\346\235\216\345\260\217\351\276\215 %u <u%u@example.com>", 100 },
	{ "UnknownCharset", "=?unknown?q?Ren=C3=A9e?= <u%u@example.com>", 100 },
	{ "QuotedUtf8", "\"Ren\303\251e\\\" M\303\274ller\" <u%u@example.com>", 100 },
	{ "QuotedEncoded", "\"=?utf-8?b?IlxOYW1lIg==?=\" <u%u@example.com>", 100 },
	{ "Utf8Emoji", "Name \360\237\230\200 %u <u%u@example.com>", 100 },
	{ "EncodedCjk", "=?utf-8?b?5p2O5bCP6b6N?= <u%u@example.com>", 100 },
	{ "InvalidEncoded", "=?utf-8?q?Ren=C3=A9e=FF?= <u%u@example.com>", 100 },
	{ "LongName", NULL, 100 },
	{ "LongUtf8", NULL, 100 },
	{ "LongEquals", NULL, 100 },
	{ "CommentNames", "u%u@example.com (User %u)", 100 },
	{ "CommentTabs", "u%u@example.com (\tFirst\t Last %u\t)", 100 },
	{ "CommentUtf8", "u%u@example.com (Ren\303\251e M\303\274ller %u)", 100 },
	{ "CommentEncoded", "u%u@example.com (=?utf-8?q?Ren=C3=A9e?= =?utf-8?q?_M=C3=BCller?=)", 100 },
	{ "LongComment", NULL, 100 },
	{ "LongCommentUtf8", NULL, 100 },
	{ "CommentUtf8Tabs", "u%u@example.com (\tRen\303\251e\tM\303\274ller %u\t)", 100 },
	{ "CommentEncodedTabs", "u%u@example.com (=?utf-8?q?Ren=C3=A9e?=\t=?utf-8?q?_M=C3=BCller?=)", 100 },
	{ "CommentUnknownCharsetTabs", "u%u@example.com (\t=?unknown?q?Ren=C3=A9e?=\t)", 100 },
	{ "LongCommentUtf8Tabs", NULL, 100 },
	{ "LongCommentEqualsTabs", NULL, 100 },
	{ "EscapedName", "\"First\\\" Last\\\\ %u\" <u%u@example.com>", 100 },
	{ "FoldedName", "\"First\r\n Last %u\" <u%u@example.com>", 100 },
	{ "LongQuotedName", NULL, 100 },
	{ "LongQuotedUtf8", NULL, 100 },
	{ "LongQuotedEquals", NULL, 100 }
};

static const AddressInput validator_inputs[] = {
	{ "SingleAddress", "Joe Sixpack <joe@example.com>", 1 },
	{ "TenRecipients", "User %u <u%u@example.com>", 10 },
	{ "HundredRecipients", "User %u <u%u@example.com>", 100 },
	{ "ThousandRecipients", "User %u <u%u@example.com>", 1000 },
	/* Note: we deliberately use Latin-1 instead of UTF-8 for validator inputs. */
	{ "InternationalDisplayNames", "Ren\351e M\374ller %u <u%u@example.com>", 100 },
	{ "QuotedDisplayNames", "\"Doe, John %u\" <u%u@example.com>", 100 },
	{ "Groups", NULL, 20 },
	{ "LongAtoms", NULL, 100 },
	{ "BareAddrspecs", "u%u@example.com", 500 },
	{ "MissingSeparators", "u%u@example.com", 500 },
	{ "ExtraneousCommas", NULL, 2000 },
	{ "UnbalancedAngleBrackets", "<<u%u@example.com>", 500 },
	{ "ControlCharacters", "a\001b %u <u%u@example.com>", 100 },
	{ "NullBytes", "a\001b %u <u%u@example.com>", 500 },
	{ "Iso2022Sequences", "\033$B%u\033(B@example.com", 500 }
};

static GString *
address_input (const AddressInput *input, gboolean validator)
{
	GString *text = g_string_new (NULL);
	char *long_name = g_strnfill (1024, 'x');
	guint i, j;

	for (i = 0; i < input->count; i++) {
		const char *name = input->name;

		if (!strcmp (name, "ExtraneousCommas")) {
			g_string_append_c (text, ',');
			continue;
		}

		if (i > 0) {
			g_string_append (text, !strcmp (name, "MissingSeparators") ? " " :
				validator && !strcmp (name, "Groups") ? "; " : ", ");
		}

		if (input->format) {
			g_string_append_printf (text, input->format, i, i, i);
		} else if (!strcmp (name, "MixedEncoded")) {
			g_string_append_printf (text, i % 2 ?
				"=?iso-8859-1?q?Ren=E9e?= <u%u@example.com>" :
				"=?utf-8?q?Ren=C3=A9e?= <u%u@example.com>", i);
		} else if (!strcmp (name, "LongAtoms")) {
			char *a = g_strnfill (48, 'a'), *b = g_strnfill (48, 'b');

			g_string_append_printf (text, "%s%u@%s.example.com", a, i, b);
			g_free (a);
			g_free (b);
		} else if (!strcmp (name, "Comment")) {
			char *comment = g_strnfill (32768, 'x');

			g_string_append_printf (text, "(%s) a@example.com", comment);
			g_free (comment);
		} else if (!strcmp (name, "Groups")) {
			g_string_append_printf (text, "Group %u: ", i);

			for (j = 0; j < 5; j++)
				g_string_append_printf (text, "%su%u_%u@example.com", j ? ", " : "", i, j);
		} else if (g_str_has_prefix (name, "Long")) {
			const char *suffix = strstr (name, "Utf8") ? "\303\251" :
				strstr (name, "Equals") ? "=?" : "";

			if (g_str_has_prefix (name, "LongComment"))
				g_string_append_printf (text, "u%u@example.com (%s%s%s)", i, long_name,
					strstr (name, "Tabs") ? "\t" : "", suffix);
			else if (g_str_has_prefix (name, "LongQuoted"))
				g_string_append_printf (text, "\"%s%s\" <u%u@example.com>", long_name, suffix, i);
			else
				g_string_append_printf (text, "%s%s <u%u@example.com>", long_name, suffix, i);
		} else {
			g_error ("Unknown address benchmark: %s", name);
		}
	}

	if (validator && !strcmp (input->name, "Groups"))
		g_string_append_c (text, ';');

	if (!strcmp (input->name, "NullBytes")) {
		for (i = 0; i < text->len; i++) {
			if (text->str[i] == '\001')
				text->str[i] = '\0';
		}
	}

	g_free (long_name);

	return text;
}

static guint64
parse_addresses (gpointer data)
{
	GMimeInternetAddressList *addresses;
	GString *text = data;
	GError *error = NULL;
	guint count;

	addresses = g_mime_internet_address_list_parse_full (NULL, text->str, text->len, &error);
	benchmark_check (addresses != NULL, "address parsing", &error);
	count = g_mime_internet_address_list_length (addresses);
	g_object_unref (addresses);

	return count;
}

static void
count_issue (const GMimeComplianceIssue *issue, gpointer data)
{
	guint64 *count = data;
	(*count)++;
}

static guint64
parse_address_string (gpointer data)
{
	GString *text = data;
	GMimeInternetAddressList *addresses = g_mime_internet_address_list_parse (NULL, text->str);
	guint count;

	if (!addresses)
		g_error ("Address string parsing failed");
	count = g_mime_internet_address_list_length (addresses);
	g_object_unref (addresses);
	return count;
}

typedef struct {
	GMimeReaderUtf8Func scan;
	const guint8 *input;
	gsize length;
} Utf8Data;

static guint64
scan_utf8 (gpointer data)
{
	Utf8Data *test = data;
	return test->scan (test->input, test->length);
}

static void
benchmark_utf8 (void)
{
	static const struct {
		const char *name, *unit;
	} inputs[] = {
		{ "Ascii", "ordinary ascii text " }, { "Latin", "caf\303\251 d\303\251j\303\240 " },
		{ "Cjk", "\346\227\245\346\234\254\350\252\236" }, { "Emoji", "\360\237\230\200\360\237\232\200" },
		{ "Mixed", "\303\251 " "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  " \346\227\245\360\237\230\200\r\n " }
	};
	static const char *errors[] = { "Valid", "Early", "Late" };
	static const guint sizes[] = { 32, 128, 512, 8192 };

	for (guint i = 0; i < G_N_ELEMENTS (inputs); i++) {
		for (guint s = 0; s < G_N_ELEMENTS (sizes); s++) {
			for (guint e = 0; e < G_N_ELEMENTS (errors); e++) {
				for (guint scalar = 0; scalar <= 1; scalar++) {
					char *name = g_strdup_printf ("Utf8Validation/%s/%u/%s/%s", inputs[i].name,
						sizes[s], errors[e], scalar ? "Scalar" : "Dispatch");
					if (benchmark_selected (name)) {
						GString *text = g_string_sized_new (sizes[s]);
						const char *invalid;
						gsize expected = sizes[s];
						Utf8Data test;

						while (text->len < sizes[s])
							g_string_append (text, inputs[i].unit);
						g_string_truncate (text, sizes[s]);
						if (!g_utf8_validate_len (text->str, text->len, &invalid))
							memset ((char *) invalid, 'a', text->len - (invalid - text->str));
						if (e) {
							text->str[e == 1 ? 0 : text->len - 1] = (char) 0xff;
							/* A damaged continuation makes its lead byte the first invalid index. */
							if (g_utf8_validate_len (text->str, text->len, &invalid))
								g_error ("Expected invalid UTF-8");
							expected = invalid - text->str;
						}
						test.scan = scalar ? _g_mime_reader_utf8_get_backend (GMIME_READER_UTF8_SCALAR) :
							_g_mime_reader_utf8_invalid_index;
						test.input = (guint8 *) text->str;
						test.length = text->len;
						if (scan_utf8 (&test) != expected)
							g_error ("%s: incorrect first invalid index", name);
						benchmark_run (name, text->len, scan_utf8, &test);
						g_string_free (text, TRUE);
					}
					g_free (name);
				}
			}
		}
	}
}

static guint64
validate_addresses (gpointer data)
{
	GString *text = data;
	guint64 count = 0;

	_g_mime_address_validate ((const guint8 *) text->str, text->len, GMIME_COMPLIANCE_VALIDATOR_ADDRESS,
		count_issue, &count, GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 5);

	return count;
}

static const char encoded_words[] =
	"I'm so happy! =?utf-8?b?8J+YgA==?= I love MIME so much =?utf-8?b?4p2k77iP4oCN8J+UpSE=?= Isn't it great?";

static guint64
decode_words (gpointer data)
{
	char *decoded = GPOINTER_TO_INT (data) ?
		g_mime_utils_header_decode_phrase (NULL, encoded_words) :
		g_mime_utils_header_decode_text (NULL, encoded_words);
	gsize length;

	if (!decoded)
		g_error ("RFC 2047 decoding failed");

	length = strlen (decoded);
	g_free (decoded);

	return length;
}

static guint64
parse_dmarc (gpointer data)
{
	GError *error = NULL;
	GMimeDmarcRecord *record = g_mime_dmarc_record_parse (data, &error);
	GMimeDmarcPolicy policy;

	benchmark_check (record != NULL, "DMARC policy parsing", &error);
	policy = g_mime_dmarc_record_get_policy (record);
	g_mime_dmarc_record_free (record);
	return policy;
}

typedef GResolver BenchmarkResolver;
typedef GResolverClass BenchmarkResolverClass;
G_DEFINE_TYPE (BenchmarkResolver, benchmark_resolver, G_TYPE_RESOLVER)

static GList *
lookup_policy (GResolver *resolver, const char *name, GResolverRecordType type, GCancellable *cancel, GError **error)
{
	const char *txt[] = { "v=DMARC1; p=reject; psd=n", NULL };

	if (type == G_RESOLVER_RECORD_TXT && !strcmp (name, "_dmarc.example.org."))
		return g_list_prepend (NULL, g_variant_ref_sink (g_variant_new ("(^as)", txt)));
	g_set_error_literal (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, "Not in benchmark zone");
	return NULL;
}

static void
benchmark_resolver_class_init (BenchmarkResolverClass *klass)
{
	klass->lookup_records = lookup_policy;
}

static void
benchmark_resolver_init (BenchmarkResolver *resolver)
{
}

typedef struct {
	GMimeDmarcVerifier *verifier;
	GMimeMessage *message;
} DmarcBenchmark;

static guint64
evaluate_dmarc (gpointer data)
{
	DmarcBenchmark *bench = data;
	GMimeDmarcIdentifier dkim = { GMIME_DMARC_IDENTIFIER_PASS, "relay.example.org" };
	GError *error = NULL;
	GMimeDmarcResult *result = g_mime_dmarc_verifier_verify_with_results (bench->verifier, bench->message,
									     &dkim, 1, NULL, NULL, &error);
	GMimeDmarcStatus status;

	benchmark_check (result != NULL, "DMARC policy evaluation", &error);
	status = g_mime_dmarc_result_get_status (result);
	g_mime_dmarc_result_free (result);
	return status;
}

void
benchmark_headers (void)
{
	static const char *policies[] = {
		"v=DMARC1; p=reject",
		"v=DMARC1; adkim=s; aspf=r; p=reject; sp=quarantine; np=reject; "
		"rua=mailto:reports@example.org,mailto:agg@example.net; "
		"ruf=mailto:fail@example.org; fo=0:d:s; psd=n"
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (policies); i++) {
		const char *name = i ? "DMARC/ReportingPolicy" : "DMARC/MinimalPolicy";

		if (benchmark_selected (name)) {
			if (parse_dmarc ((gpointer) policies[i]) != GMIME_DMARC_POLICY_REJECT)
				g_error ("%s: unexpected DMARC policy", name);
			benchmark_run (name, strlen (policies[i]), parse_dmarc, (gpointer) policies[i]);
		}
	}
	if (benchmark_selected ("DMARC/RelaxedTreeWalk")) {
		GResolver *resolver = g_object_new (benchmark_resolver_get_type (), NULL);
		DmarcBenchmark bench = { g_mime_dmarc_verifier_new (resolver), g_mime_message_new (FALSE) };
		g_mime_message_add_mailbox (bench.message, GMIME_ADDRESS_TYPE_FROM, NULL, "a@news.example.org", NULL);
		if (evaluate_dmarc (&bench) != GMIME_DMARC_STATUS_PASS)
			g_error ("DMARC: expected aligned DKIM through shared organizational domain");
		benchmark_run ("DMARC/RelaxedTreeWalk", 0, evaluate_dmarc, &bench);
		g_object_unref (bench.message);
		g_object_unref (bench.verifier);
		g_object_unref (resolver);
	}

	benchmark_utf8 ();
	for (i = 0; i < G_N_ELEMENTS (parser_inputs); i++) {
		const AddressInput *input = &parser_inputs[i];
		char *name = g_strdup_printf ("AddressParser/%s", input->name);

		if (benchmark_selected (name)) {
			GString *text = address_input (input, FALSE);

			if (parse_addresses (text) != input->count)
				g_error ("%s: unexpected number of parsed addresses", name);

			benchmark_run (name, text->len, parse_addresses, text);
			g_string_free (text, TRUE);
		}

		g_free (name);
		name = g_strdup_printf ("AddressParser/String/%s", input->name);
		if (benchmark_selected (name)) {
			GString *text = address_input (input, FALSE);
			if (parse_address_string (text) != input->count)
				g_error ("%s: unexpected number of parsed addresses", name);
			benchmark_run (name, text->len, parse_address_string, text);
			g_string_free (text, TRUE);
		}
		g_free (name);
	}

	for (i = 0; i < G_N_ELEMENTS (validator_inputs); i++) {
		const AddressInput *input = &validator_inputs[i];
		char *name = g_strdup_printf ("AddressValidator/%s", input->name);

		if (benchmark_selected (name)) {
			GString *text = address_input (input, TRUE);
			guint64 issues = validate_addresses (text);
			gboolean valid = i <= 8 && i != 4 && i != 6;

			if ((valid && issues != 0) || (!valid && issues == 0))
				g_error ("%s: unexpected compliance result (%" G_GUINT64_FORMAT " issues)", name, issues);

			benchmark_run (name, text->len, validate_addresses, text);
			g_string_free (text, TRUE);
		}

		g_free (name);
	}

	for (i = 0; i < 2; i++) {
		const char *name = i ? "Rfc2047/DecodePhrase" : "Rfc2047/DecodeText";

		if (benchmark_selected (name)) {
			char *decoded = i ? g_mime_utils_header_decode_phrase (NULL, encoded_words) :
				g_mime_utils_header_decode_text (NULL, encoded_words);
			const char *expected = "I'm so happy! \360\237\230\200 I love MIME so much "
				"\342\235\244\357\270\217\342\200\215\360\237\224\245! Isn't it great?";

			if (!decoded || strcmp (decoded, expected))
				g_error ("%s: incorrect decoded text", name);

			g_free (decoded);

			benchmark_run (name, sizeof (encoded_words) - 1, decode_words, GINT_TO_POINTER (i));
		}
	}
}
