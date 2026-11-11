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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-address-validator-private.h>
#include <gmime/gmime-encoded-word-validator-private.h>

#include "testsuite.h"

#define INVALID GMIME_COMPLIANCE_INVALID_ENCODED_WORD
#define PLACEMENT GMIME_COMPLIANCE_INVALID_ENCODED_WORD_PLACEMENT
#define WORDS GMIME_COMPLIANCE_VALIDATOR_ENCODED_WORDS
#define ADDRESS GMIME_COMPLIANCE_VALIDATOR_ADDRESS

static void
collect_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
	g_array_append_val ((GArray *) user_data, *issue);
}

static guint
count_issues (GArray *issues, GMimeComplianceViolation violation)
{
	guint i, count = 0;

	for (i = 0; i < issues->len; i++)
		count += g_array_index (issues, GMimeComplianceIssue, i).violation == violation;
	return count;
}

static void
assert_words (GArray *issues, guint invalid, guint placement)
{
	guint i;

	g_assert_cmpuint (count_issues (issues, INVALID), ==, invalid);
	g_assert_cmpuint (count_issues (issues, PLACEMENT), ==, placement);
	for (i = 0; i < issues->len; i++) {
		GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

		if (issue->violation == INVALID || issue->violation == PLACEMENT)
			g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_ELEMENT_START);
	}
}

static GMimeStream *
new_input (const char *wire, gboolean chunked)
{
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (wire, strlen (wire));
	GMimeStream *stream;

	if (!chunked)
		return input;
	stream = test_stream_onebyte_new (input);
	g_object_unref (input);
	return stream;
}

static GArray *
read_issues (const char *wire, gboolean chunked, GMimeComplianceOptions *options)
{
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GMimeStream *stream = new_input (wire, chunked);
	GMimeReader *reader = g_mime_reader_new (NULL, stream);
	GError *error = NULL;

	g_mime_reader_set_compliance_options (reader, options);
	g_mime_reader_set_compliance_logger (reader, collect_issue, issues, NULL);
	g_assert_true (g_mime_reader_read_message (reader, &error));
	g_assert_no_error (error);
	g_object_unref (reader);
	g_object_unref (stream);
	return issues;
}

static void
assert_header (const char *field, const char *value, guint invalid, guint placement)
{
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	char *wire = g_strdup_printf ("%s: %s\r\n\r\nbody\r\n", field, value);
	guint chunked, syntax;

	for (syntax = 0; syntax < 2; syntax++) {
		g_mime_compliance_options_set_enabled_validators (options, syntax ? GMIME_COMPLIANCE_VALIDATOR_ALL : WORDS);
		for (chunked = 0; chunked < 2; chunked++) {
			GArray *issues = read_issues (wire, chunked, options);

			assert_words (issues, invalid, placement);
			g_array_unref (issues);
		}
	}
	g_mime_compliance_options_unref (options);
	g_free (wire);
}

static void
test_syntax (void)
{
	/* Ported from MimeKit's regressions inspired by noxxi/mime-is-broken. */
	static const struct {
		const char *value;
		guint invalid;
	} cases[] = {
		{ "ordinary text", 0 },
		{ "=?utf-8?Q?caf=C3=A9?=", 0 },
		{ "=?UTF-8?q?hello_world?=", 0 },
		{ "=?utf-8?B?YQ==?=", 0 },
		{ "=?utf-8?b?YWI=?=", 0 },
		{ "=?utf-8?B?YWJj?=", 0 },
		{ "=?utf-8?B?Zh==?=", 1 },
		{ "=?utf-8?b?Zm9=?=", 1 },
		{ "=?utf-8?B?AAA/?=", 0 },
		{ "=?utf-8?Q?Zh=3D=3D?=", 0 },
		{ "=?x-unknown*en?Q?hello?=", 0 },
		{ "=?utf-8*?Q?hello?=", 0 },
		{ "=?utf-8?Q?first?=\r\n\t=?utf-8?Q?second?=", 0 },
		{ "=?utf-8?Q?()\\:.,?=", 0 },
		{ "=?utf-8?Q?=ff=00?=", 0 },
		{ "=?", 1 },
		{ "=?utf-8?Q?unfinished", 1 },
		{ "=??Q?hello?=", 1 },
		{ "=?*en?Q?hello?=", 1 },
		{ "=?utf.8?Q?hello?=", 1 },
		{ "=?utf 8?Q?hello?=", 1 },
		{ "=?utf-8?X?hello?=", 1 },
		{ "=?utf-8?QQ?hello?=", 1 },
		{ "=?utf-8??hello?=", 1 },
		{ "=?utf-8?Q?" "?=", 1 },
		{ "=?utf-8?Q?a b?=", 1 },
		{ "=?utf-8?Q?a\tb?=", 1 },
		{ "=?utf-8?Q?a\r\n b?=", 1 },
		{ "=?utf-8?Q?caf\303\251?=", 1 },
		{ "=?utf-8?Q?bad=GG?=", 1 },
		{ "=?utf-8?Q?bad=A?=", 1 },
		{ "=?utf-8?Q?bad=?=", 1 },
		{ "=?utf-8?B?YQ?=", 1 },
		{ "=?utf-8?B?YQ=?=", 1 },
		{ "=?utf-8?B?Y===?=", 1 },
		{ "=?utf-8?B?=WJj?=", 1 },
		{ "=?utf-8?B?YWJj!?=", 1 },
		{ "=?utf-8?B?YQ==YWJj?=", 1 },
		{ "prefix=?utf-8?Q?hello?=", 1 },
		{ "=?utf-8?Q?hello?=suffix", 1 },
		{ "=?utf-8?Q?first?==?utf-8?Q?second?=", 2 },
		{ "=?broken=?utf-8?Q?hello?=", 2 },
		{ "=?utf-8?Q?broken=?utf-8?Q?hello?=", 2 },
		{ "(=?utf-8?Q?hello?=)", 1 },
		{ "=?utf-\377?Q?hello?=", 1 },
		{ "=?utf-8?\377?hello?=", 1 }
	};
	const char *fields[] = { "Subject", "Comments", "Content-Description", "X-Example", "x-example" };
	guint i, field, length;

	testsuite_check ("Encoded-word syntax, payloads and separation in selected unstructured headers");
	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		for (field = 0; field < G_N_ELEMENTS (fields); field++)
			assert_header (fields[field], cases[i].value, cases[i].invalid, 0);
	for (length = 63; length <= 64; length++) {
		char *payload = g_strnfill (length, 'a');
		char *word = g_strdup_printf ("=?utf-8?Q?%s?=", payload);

		g_assert_cmpuint (strlen (word), ==, length + 12);
		for (field = 0; field < G_N_ELEMENTS (fields); field++)
			assert_header (fields[field], word, length == 64, 0);
		g_free (payload);
		g_free (word);
	}
	testsuite_check_passed ();
}

static void
test_addresses (void)
{
	static const struct {
		const char *value;
		guint invalid, placement;
	} cases[] = {
		{ "=?utf-8?Q?hello_world?= <a@example.com>", 0, 0 },
		{ "=?utf-8?B?YQ==?= <a@example.com>", 0, 0 },
		{ "=?utf-8?B?Zh==?= <a@example.com>", 1, 0 },
		{ "=?utf-8?B?Zm9=?= <a@example.com>", 1, 0 },
		{ "a@example.com (=?utf-8?B?Zh==?=)", 1, 0 },
		{ "a@example.com (=?utf-8?B?Zm9=?=)", 1, 0 },
		{ "\"=?utf-8?B?Zh==?=\" <a@example.com>", 1, 1 },
		{ "\"=?utf-8?B?Zm9=?=\" <a@example.com>", 1, 1 },
		{ "=?utf-8?Q?group?= : a@example.com;", 0, 0 },
		{ "a@example.com (=?utf-8?Q?hello?=)", 0, 0 },
		{ "(outer (=?utf-8?Q?hello?=)) a@example.com", 0, 0 },
		{ "a (=?utf-8?Q?hello?=) @ example.com", 0, 0 },
		{ "=?utf-8?Q?hello?= (=?utf-8?Q?comment?=) <a@example.com>", 0, 0 },
		{ "=?utf-8?Q?bad=GG?= <a@example.com>", 1, 0 },
		{ "=?utf-8?B?YQ?= <a@example.com>", 1, 0 },
		{ "=?utf-8?X?hello?= <a@example.com>", 1, 0 },
		{ "=?utf-8?Q?hello.world?= <a@example.com>", 1, 0 },
		{ "=?utf-8?Q?hello=2Eworld?= <a@example.com>", 0, 0 },
		{ "=?utf-8?Q?group.name?= : a@example.com;", 1, 0 },
		{ "=?utf-8?Q?hello?=<a@example.com>", 1, 0 },
		{ "=?utf-8?Q?group?=: a@example.com;", 1, 0 },
		{ "prefix=?utf-8?Q?hello?= <a@example.com>", 1, 0 },
		{ "a@example.com (=?utf-8?Q?hello.world?=)", 0, 0 },
		{ "a@example.com (=?utf-8?Q?hello(world)?=)", 1, 0 },
		{ "a@example.com (=?utf-8?Q?hello\\world?=)", 1, 0 },
		{ "a@example.com (prefix=?utf-8?Q?hello?=)", 1, 0 },
		{ "a@example.com (=?utf-8?Q?bad=GG?=)", 1, 0 },
		{ "\"=?utf-8?Q?hello?=\" <a@example.com>", 0, 1 },
		{ "\"=?utf-8?Q?bad=GG?=\" <a@example.com>", 1, 1 },
		{ "\"=?utf-8?Q?bad=?=\" <a@example.com>", 1, 1 },
		{ "\"=?utf-8?Q?group?=\" : a@example.com;", 0, 1 },
		{ "=?utf-8?Q?user?=@example.com", 0, 1 },
		{ "\"=?utf-8?Q?user?=\"@example.com", 0, 1 },
		{ "a@=?utf-8?Q?example?=.com", 0, 1 },
		{ "a@=?utf-8?Q?example.com?=", 0, 1 },
		{ "a@[=?utf-8?Q?example?=]", 0, 1 },
		{ "Name <a@=?utf-8?Q?example?=.com>", 0, 1 },
		{ "=?literal@example.com", 0, 0 },
		{ "\"=?literal\"@example.com", 0, 0 },
		{ "a@example.com, =?utf-8?Q?bad=GG?= <b@example.com>", 1, 0 },
		{ "=?utf-8?Q?group?= : =?utf-8?Q?bad=GG?= <a@example.com>;", 1, 0 },
		{ "=?utf-8?Q?first?= <a@example.com>, \"=?utf-8?Q?second?=\" <b@example.com>", 0, 1 },
		{ "(=?utf-8?Q?bad=GG?=) \"=?utf-8?Q?hello?=\" <a@example.com>", 1, 1 },
		{ "(\\=?utf-8?Q?bad=GG?=) a@example.com", 0, 0 },
		{ "\"\\=?utf-8?Q?hello?=\" <a@example.com>", 0, 0 }
	};
	const char *fields[] = {
		"From", "Sender", "Reply-To", "Cc", "Bcc", "Resent-From", "Resent-Sender",
		"Resent-Reply-To", "Resent-To", "Resent-Cc", "Resent-Bcc", "Disposition-Notification-To"
	};
	const char *excluded[] = { "Date", "Message-Id", "Received", "Content-Type", "Content-Disposition", "Return-Path" };
	guint i;

	testsuite_check ("Address phrases, comments, quoted strings and addr-spec encoded-word placement");
	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		assert_header ("To", cases[i].value, cases[i].invalid, cases[i].placement);
	for (i = 0; i < G_N_ELEMENTS (fields); i++)
		assert_header (fields[i], "=?utf-8?Q?bad=GG?= <a@example.com>", 1, 0);
	for (i = 0; i < G_N_ELEMENTS (excluded); i++)
		assert_header (excluded[i], "=?utf-8?Q?bad=GG?=", 0, 0);
	testsuite_check_passed ();
}

static void
test_base64_pad_bits (void)
{
	const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	testsuite_check ("Encoded-word Base64 pad bits cover every sextet with bounded slices and original positions");
	for (guint padding = 1; padding <= 2; padding++) {
		for (guint rank = 0; rank < 64; rank++) {
			char *wire = g_strdup_printf ("xx=?utf-8?B?%s%c%s?=yy",
				padding == 2 ? "A" : "AA", alphabet[rank], padding == 2 ? "==" : "=");
			GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
			GMimeEncodedWordValidator validator;
			gboolean canonical = rank % (padding == 2 ? 16 : 4) == 0;

			_g_mime_encoded_word_validator_init (&validator, (const guint8 *) wire + 2, strlen (wire) - 4,
				collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_STORAGE, 100, 5, 7);
			_g_mime_encoded_word_validator_validate (&validator, strlen (wire) - 4, GMIME_ENCODED_WORD_CONTEXT_TEXT);
			assert_words (issues, canonical ? 0 : 1, 0);
			g_assert_cmpuint (issues->len, ==, canonical ? 0 : 1);
			if (!canonical) {
				GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, 0);

				g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
				g_assert_cmpint (issue->stream_offset, ==, 100);
				g_assert_cmpuint (issue->line_number, ==, 5);
				g_assert_cmpuint (issue->column_number, ==, 7);
			}
			g_array_unref (issues);
			g_free (wire);
		}
	}
	testsuite_check_passed ();
}

static void
test_base64_pad_bits_gating (void)
{
	const char wire[] = "Subject: =?utf-8?B?Zh==?=\r\nTo: =?utf-8?B?Zm9=?= <a@example.com>\r\n"
		"Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: base64\r\n\r\nZm9=\r\n";
	const GMimeComplianceValidators flags[] = {
		GMIME_COMPLIANCE_VALIDATOR_NONE, GMIME_COMPLIANCE_VALIDATOR_BASE64, WORDS, GMIME_COMPLIANCE_VALIDATOR_ALL,
		GMIME_COMPLIANCE_VALIDATOR_ALL & ~GMIME_COMPLIANCE_VALIDATOR_BASE64, GMIME_COMPLIANCE_VALIDATOR_ALL & ~WORDS
	};
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	GByteArray *baseline = NULL;

	testsuite_check ("Base64 pad-bit flags and logging preserve decoded headers, content and serialization");
	for (guint chunked = 0; chunked < 2; chunked++) {
		for (guint flag = 0; flag < G_N_ELEMENTS (flags); flag++) {
			g_mime_compliance_options_set_enabled_validators (options, flags[flag]);
			for (guint logging = 0; logging < 2; logging++) {
				GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
				GMimeStream *input = new_input (wire, chunked), *output = g_mime_stream_mem_new ();
				GMimeStream *decoded = g_mime_stream_mem_new ();
				GMimeParser *parser = g_mime_parser_new_with_stream (input);
				GMimeMessage *message;
				GMimeInternetAddress *address;
				GMimeEntity *part;
				GByteArray *bytes;
				GError *error = NULL;
				gboolean words = logging && (flags[flag] & WORDS);
				gboolean body = logging && (flags[flag] & GMIME_COMPLIANCE_VALIDATOR_BASE64);

				g_mime_parser_set_compliance_options (parser, options);
				if (logging)
					g_mime_parser_set_compliance_logger (parser, collect_issue, issues, NULL);
				message = g_mime_parser_construct_message (parser, NULL, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_cmpstr (g_mime_message_get_subject (message), ==, "f");
				address = g_mime_internet_address_list_get_address (g_mime_message_get_to (message), 0);
				g_assert_true (GMIME_IS_MAILBOX_ADDRESS (address));
				g_assert_cmpstr (g_mime_internet_address_get_name (address), ==, "fo");
				part = g_mime_message_get_mime_part (message);
				g_assert_true (GMIME_IS_PART (part));
				g_assert_cmpint (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (part)), decoded), ==, 2);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (decoded));
				g_assert_cmpmem (bytes->data, bytes->len, "fo", 2);
				g_assert_cmpint (g_mime_entity_write_to_stream (GMIME_ENTITY (message), NULL, output), >, 0);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				if (!baseline)
					baseline = g_byte_array_ref (bytes);
				g_assert_cmpmem (bytes->data, bytes->len, baseline->data, baseline->len);
				assert_words (issues, words ? 2 : 0, 0);
				g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_INVALID_BASE64_PADDING), ==, body ? 1 : 0);
				for (guint i = 0; i < issues->len; i++) {
					GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

					if (issue->violation == GMIME_COMPLIANCE_INVALID_BASE64_PADDING) {
						g_assert_cmpint (issue->stream_offset, ==, strrchr (wire, '=') - wire);
						g_assert_cmpuint (issue->line_number, ==, 6);
						g_assert_cmpuint (issue->column_number, ==, 4);
						g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_EXACT);
					}
				}
				g_array_unref (issues);
				g_object_unref (message);
				g_object_unref (parser);
				g_object_unref (input);
				g_object_unref (output);
				g_object_unref (decoded);
			}
		}
	}
	g_byte_array_unref (baseline);
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_gating_and_preservation (void)
{
	const char *wire = "Subject: =?utf-8?Q?bad=GG?=\r\n"
		"To: =?x-unknown?Q?hello?= <a@example.com>, \"=?utf-8?Q?other?=\" <b@example.com>\r\n"
		"To: =?utf-8?Q?ignored=GG?= <ignored@example.com>\r\n\r\nbody\r\n";
	const GMimeComplianceValidators flags[] = {
		GMIME_COMPLIANCE_VALIDATOR_NONE, ADDRESS, WORDS, GMIME_COMPLIANCE_VALIDATOR_ALL,
		GMIME_COMPLIANCE_VALIDATOR_ALL & ~WORDS
	};
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	GByteArray *baseline = NULL;
	char *subject = NULL;
	int addresses = 0;
	guint chunked, flag, logging;

	testsuite_check ("Encoded-word gating preserves subjects, recipients and serialization, including ignored headers");
	for (chunked = 0; chunked < 2; chunked++) {
		for (flag = 0; flag < G_N_ELEMENTS (flags); flag++) {
			g_mime_compliance_options_set_enabled_validators (options, flags[flag]);
			for (logging = 0; logging < 2; logging++) {
				GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
				GMimeStream *input = new_input (wire, chunked), *output = g_mime_stream_mem_new ();
				GMimeParser *parser = g_mime_parser_new_with_stream (input);
				GMimeMessage *message;
				GByteArray *bytes;
				GError *error = NULL;
				gboolean enabled = logging && (flags[flag] & WORDS);

				g_mime_parser_set_compliance_options (parser, options);
				if (logging)
					g_mime_parser_set_compliance_logger (parser, collect_issue, issues, NULL);
				message = g_mime_parser_construct_message (parser, NULL, &error);
				g_assert_no_error (error);
				g_assert_nonnull (message);
				g_assert_cmpint (g_mime_entity_write_to_stream (GMIME_ENTITY (message), NULL, output), >, 0);
				bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
				if (baseline) {
					g_assert_cmpmem (bytes->data, bytes->len, baseline->data, baseline->len);
					g_assert_cmpstr (g_mime_message_get_subject (message), ==, subject);
					g_assert_cmpint (g_mime_internet_address_list_length (g_mime_message_get_to (message)), ==, addresses);
				} else {
					baseline = g_byte_array_ref (bytes);
					subject = g_strdup (g_mime_message_get_subject (message));
					addresses = g_mime_internet_address_list_length (g_mime_message_get_to (message));
				}
				assert_words (issues, enabled ? 2 : 0, enabled ? 1 : 0);
				g_array_unref (issues);
				g_object_unref (message);
				g_object_unref (parser);
				g_object_unref (input);
				g_object_unref (output);
			}
		}
	}
	g_byte_array_unref (baseline);
	g_free (subject);
	for (flag = 0; flag < 3; flag++) {
		GArray *issues;
		GMimeComplianceValidators validators = flag == 0 ? WORDS : flag == 1 ? ADDRESS : WORDS | ADDRESS;

		g_mime_compliance_options_set_enabled_validators (options, validators);
		issues = read_issues ("To: =?utf-8?Q?bad=GG?= <local>\r\n\r\nbody\r\n", TRUE, options);
		assert_words (issues, (validators & WORDS) != 0, 0);
		g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_ADDRESS_WITHOUT_DOMAIN), ==, (validators & ADDRESS) != 0);
		g_array_unref (issues);
	}
	/* Parameter usage remains the parameter validator's responsibility. */
	for (flag = 0; flag < 2; flag++) {
		GArray *issues;

		g_mime_compliance_options_set_enabled_validators (options, flag ? GMIME_COMPLIANCE_VALIDATOR_PARAMETERS : WORDS);
		issues = read_issues ("Content-Disposition: attachment; filename=\"=?utf-8?Q?bad=GG?=\"\r\n\r\n", TRUE, options);
		assert_words (issues, 0, 0);
		g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_ENCODED_WORD_IN_PARAMETER), ==, flag);
		g_array_unref (issues);
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_positions_and_budgets (void)
{
	const char *wire = "X-Prefix: \303\251\r\nTo: (\303\251) =?utf-8?Q?bad=GG?= <a@example.com>,\r\n"
		"\t\"=?utf-8?Q?other?=\" <b@example.com>\r\nSubject: =?utf-8?Q?bad=G?=\r\n\r\nbody\r\n";
	const char *markers[] = { "=?utf-8?Q?bad=GG", "=?utf-8?Q?other", "=?utf-8?Q?bad=G?" };
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint budget, chunked, i, found;

	testsuite_check ("Encoded-word byte positions, folded coordinates and independent issue budgets");
	g_mime_compliance_options_set_enabled_validators (options, WORDS);
	g_mime_compliance_options_set_context (options, GMIME_COMPLIANCE_CONTEXT_STORAGE);
	for (budget = 0; budget < 2; budget++) {
		g_mime_compliance_options_set_max_issues_per_violation (options, budget);
		for (chunked = 0; chunked < 2; chunked++) {
			GArray *issues = read_issues (wire, chunked, options);

			assert_words (issues, budget ? 1 : 2, 1);
			for (i = 0, found = 0; i < issues->len; i++) {
				GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);
				guint64 line = 1, column = 1;
				gsize offset, at;

				if (issue->violation != INVALID && issue->violation != PLACEMENT)
					continue;
				offset = strstr (wire, markers[found++]) - wire;
				for (at = 0; at < offset; at++) {
					if (wire[at] == '\n') {
						line++;
						column = 1;
					} else {
						column++;
					}
				}
				g_assert_cmpint (issue->stream_offset, ==, offset);
				g_assert_cmpuint (issue->line_number, ==, line);
				g_assert_cmpuint (issue->column_number, ==, column);
				g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
			}
			g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES), ==, budget);
			g_array_unref (issues);
		}
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_bounds_and_reuse (void)
{
	const char value[] = "=?utf-8?Q?bad=GG?= <a@example.com>,\r\n \"=?utf-8?Q?other?=\" <b@example.com>";
	const char partial[] = "xx=?utf-8?Q?hello?=yy";
	const char embedded[] = "=?utf-8?Q?a\0b?=";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GMimeEncodedWordValidator words;
	GMimeComplianceIssue *issue;
	guint8 *buffer = g_memdup2 (value, sizeof (value) - 1);
	guint repeat;
	const guint64 line = G_GUINT64_CONSTANT (5000000000), column = G_GUINT64_CONSTANT (6000000000);
	const gint64 offset = G_GINT64_CONSTANT (7000000000);

	testsuite_check ("Bounded encoded-word buffers, truncated envelopes, embedded NULs and wide coordinates");
	for (repeat = 0; repeat < 2; repeat++) {
		_g_mime_address_validate (buffer, sizeof (value) - 1, WORDS, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_STORAGE, offset, line, column);
		assert_words (issues, 1, 1);
		g_assert_cmpuint (issues->len, ==, 2);
		issue = &g_array_index (issues, GMimeComplianceIssue, 0);
		g_assert_cmpint (issue->stream_offset, ==, offset);
		g_assert_cmpuint (issue->line_number, ==, line);
		g_assert_cmpuint (issue->column_number, ==, column);
		issue = &g_array_index (issues, GMimeComplianceIssue, 1);
		g_assert_cmpint (issue->stream_offset, ==, offset + (strstr (value, "=?utf-8?Q?other") - value));
		g_assert_cmpuint (issue->line_number, ==, line + 1);
		g_assert_cmpuint (issue->column_number, ==, 3);
		g_array_set_size (issues, 0);

		_g_mime_encoded_word_validator_init (&words, (const guint8 *) partial + 2, sizeof (partial) - 6,
			collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_STORAGE, offset, line, column);
		_g_mime_encoded_word_validator_validate (&words, words.length, GMIME_ENCODED_WORD_CONTEXT_TEXT);
		assert_words (issues, 1, 0);
		g_assert_cmpint (g_array_index (issues, GMimeComplianceIssue, 0).stream_offset, ==, offset);
		_g_mime_encoded_word_validator_validate (&words, words.length, GMIME_ENCODED_WORD_CONTEXT_TEXT);
		assert_words (issues, 1, 0);
		g_array_set_size (issues, 0);
		_g_mime_encoded_word_validator_init (&words, (const guint8 *) partial + 2, sizeof (partial) - 5,
			collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_STORAGE, offset, line, column);
		_g_mime_encoded_word_validator_validate (&words, words.length, GMIME_ENCODED_WORD_CONTEXT_TEXT);
		g_assert_cmpuint (issues->len, ==, 0);
	}
	_g_mime_encoded_word_validator_init (&words, (const guint8 *) embedded, sizeof (embedded) - 1,
		collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_STORAGE, 0, 1, 1);
	_g_mime_encoded_word_validator_validate (&words, words.length, GMIME_ENCODED_WORD_CONTEXT_TEXT);
	assert_words (issues, 1, 0);
	g_array_unref (issues);
	g_free (buffer);
	testsuite_check_passed ();
}

static void
test_incremental_positions (void)
{
	const char *values[] = {
		"ordinary text\r\n =?utf-8?Q?valid?=\r\n\t=?utf-8?Q?bad\r\n value?=\r\n =?utf-8?B?YQ?=",
		"=?utf-8?Q?valid?= (escaped \\) \\\" text\r\n =?utf-8?Q?bad=GG?=)\r\n \"=?utf-8?Q?bad=G?=\""
	};
	const char *markers[][3] = {
		{ "=?utf-8?Q?bad\r\n value?=", "=?utf-8?B?YQ?=", NULL },
		{ "=?utf-8?Q?bad=GG?=", "=?utf-8?Q?bad=G?=", "=?utf-8?Q?bad=G?=" }
	};
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	guint address, incremental, i;

	testsuite_check ("Lazy coordinates match whole and incremental scans across folds and quoted pairs");
	for (address = 0; address < 2; address++) {
		gsize length = strlen (values[address]);
		char *slice = g_strdup_printf ("xx%syy", values[address]);

		for (incremental = 0; incremental < 2; incremental++) {
			GMimeEncodedWordValidator words;
			GMimeEncodedWordContext context = address ? GMIME_ENCODED_WORD_CONTEXT_PHRASE : GMIME_ENCODED_WORD_CONTEXT_TEXT;

			_g_mime_encoded_word_validator_init (&words, (const guint8 *) slice + 2, length,
				collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_STORAGE, 100, 5, 7);
			if (incremental) {
				for (gsize end = 0; end <= length; end++)
					_g_mime_encoded_word_validator_validate (&words, end, context);
			} else {
				_g_mime_encoded_word_validator_validate (&words, length, context);
			}
			g_assert_cmpuint (issues->len, ==, address ? 3 : 2);
			for (i = 0; i < issues->len; i++) {
				GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);
				gsize offset = strstr (values[address], markers[address][i]) - values[address];
				guint64 line = 5, column = 7;

				for (gsize at = 0; at < offset; at++) {
					if (values[address][at] == '\n') {
						line++;
						column = 1;
					} else {
						column++;
					}
				}
				g_assert_cmpint (issue->violation, ==, address && i == 1 ? PLACEMENT : INVALID);
				g_assert_cmpint (issue->stream_offset, ==, 100 + offset);
				g_assert_cmpuint (issue->line_number, ==, line);
				g_assert_cmpuint (issue->column_number, ==, column);
				g_assert_cmpint (issue->context, ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
				g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			}
			g_array_set_size (issues, 0);
		}
		g_free (slice);
	}
	g_array_unref (issues);
	testsuite_check_passed ();
}

static void
test_plain_slices (void)
{
	const char *values[] = { "", "=", "plain = text\r\n without words", "plain text =", "====" };
	const char *outside = "=?utf-8?Q?bad=GG?=";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GMimeEncodedWordValidator words;
	guint address, i;

	testsuite_check ("Whole-buffer marker probes respect slices, empty ranges and trailing equals");
	for (address = 0; address < 2; address++) {
		GMimeEncodedWordContext context = address ? GMIME_ENCODED_WORD_CONTEXT_PHRASE : GMIME_ENCODED_WORD_CONTEXT_TEXT;

		for (i = 0; i < G_N_ELEMENTS (values); i++) {
			gsize length = strlen (values[i]);
			char *slice = g_strdup_printf ("%s%s%s", outside, values[i], outside);

			_g_mime_encoded_word_validator_init (&words, (const guint8 *) slice + strlen (outside), length,
				collect_issue, issues, GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
			for (gsize end = 0; end <= length; end++)
				_g_mime_encoded_word_validator_validate (&words, end, context);
			g_assert_cmpuint (issues->len, ==, 0);
			g_assert_cmpuint (words.index, ==, length);
			g_assert_cmpuint (words.scan_index, ==, 0);
			g_free (slice);
		}
	}
	g_array_unref (issues);
	testsuite_check_passed ();
}

static void
count_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
	guint *count = user_data;

	g_assert_true (issue->violation == INVALID || issue->violation == PLACEMENT);
	(*count)++;
}

static void
test_limits_and_scale (void)
{
	const char *headers[] = { "Subject: =?utf-8?Q?bad=GG?= ", "To: =?utf-8?Q?bad=GG?= <a@" };
	const char *fragments[] = { "plain", "=?", "=?utf-8?Q?broken", "=?x-unknown?Q?bad=GG?= " };
	GMimeParserOptions *options = g_mime_parser_options_new ();
	guint i, address, chunked;

	testsuite_check ("Header limits precede validation and overlapping candidates scan in bounded time");
	g_mime_parser_options_set_max_header_length (options, 128);
	for (i = 0; i < G_N_ELEMENTS (headers); i++) {
		for (chunked = 0; chunked < 2; chunked++) {
			GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
			char *padding = g_strnfill (256, 'a');
			char *wire = g_strdup_printf ("%s%s\r\n\r\n", headers[i], padding);
			GMimeStream *input = new_input (wire, chunked);
			GMimeReader *reader = g_mime_reader_new (options, input);
			GError *error = NULL;

			g_mime_reader_set_compliance_logger (reader, collect_issue, issues, NULL);
			g_assert_false (g_mime_reader_read_message (reader, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
			assert_words (issues, 0, 0);
			g_array_unref (issues);
			g_object_unref (reader);
			g_object_unref (input);
			g_free (padding);
			g_free (wire);
		}
	}
	g_mime_parser_options_unref (options);
	for (i = 0; i < G_N_ELEMENTS (fragments); i++) {
		for (address = 0; address < 2; address++) {
			GString *text = g_string_new (NULL);
			GMimeEncodedWordValidator words;
			guint count = 0, repetitions = 0;
			gint64 start;

			while (text->len < 1024 * 1024) {
				g_string_append (text, fragments[i]);
				repetitions++;
			}
			if (address)
				g_string_append (text, " <a@example.com>");
			start = g_get_monotonic_time ();
			if (address) {
				_g_mime_address_validate ((const guint8 *) text->str, text->len, WORDS, count_issue, &count,
					GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
			} else {
				_g_mime_encoded_word_validator_init (&words, (const guint8 *) text->str, text->len,
					count_issue, &count, GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
				_g_mime_encoded_word_validator_validate (&words, text->len, GMIME_ENCODED_WORD_CONTEXT_TEXT);
			}
			g_assert_cmpint (g_get_monotonic_time () - start, <, 5 * G_USEC_PER_SEC);
			g_assert_cmpuint (count, ==, i == 0 ? 0 : repetitions);
			g_string_free (text, TRUE);
		}
	}
	testsuite_check_passed ();
}

static void
test_metadata (void)
{
	const GMimeComplianceViolation violations[] = { INVALID, PLACEMENT };
	const char *names[] = { "InvalidEncodedWord", "InvalidEncodedWordPlacement" };
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint i, context;

	testsuite_check ("Encoded-word flag, numeric identities and context-sensitive metadata match MimeKit");
	g_assert_cmpuint (WORDS, ==, 32);
	g_assert_cmpuint (GMIME_COMPLIANCE_VALIDATOR_ALL, ==, 63);
	g_assert_cmpuint (g_mime_compliance_options_get_enabled_validators (options), ==, GMIME_COMPLIANCE_VALIDATOR_ALL);
	for (i = 0; i < G_N_ELEMENTS (violations); i++) {
		g_assert_cmpuint (violations[i], ==, 52 + i);
		g_assert_cmpstr (g_mime_compliance_violation_get_name (violations[i]), ==, names[i]);
		g_assert_nonnull (g_mime_compliance_violation_get_description (violations[i]));
		g_assert_nonnull (g_mime_compliance_violation_get_remarks (violations[i]));
		g_assert_cmpint (g_mime_compliance_violation_get_categories (violations[i]), ==,
				GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_SECURITY);
		for (context = GMIME_COMPLIANCE_CONTEXT_TRANSPORT; context <= GMIME_COMPLIANCE_CONTEXT_STORAGE; context++)
			g_assert_cmpint (g_mime_compliance_violation_get_severity (violations[i], context), ==, GMIME_COMPLIANCE_SEVERITY_MAJOR);
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Encoded-word compliance");
	test_syntax ();
	test_addresses ();
	test_base64_pad_bits ();
	test_base64_pad_bits_gating ();
	test_gating_and_preservation ();
	test_positions_and_budgets ();
	test_bounds_and_reuse ();
	test_incremental_positions ();
	test_plain_slices ();
	test_limits_and_scale ();
	test_metadata ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
