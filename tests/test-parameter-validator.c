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

#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-parameter-validator-private.h>

#include "testsuite.h"

#define BOUNDARY GMIME_COMPLIANCE_REPEATED_BOUNDARY_PARAMETER
#define FILENAME GMIME_COMPLIANCE_REPEATED_FILENAME_PARAMETER
#define PARAMETER GMIME_COMPLIANCE_REPEATED_PARAMETER
#define DISPOSITION GMIME_COMPLIANCE_REPEATED_CONTENT_DISPOSITION
#define MISSING_SEGMENT GMIME_COMPLIANCE_MISSING_PARAMETER_SEGMENT
#define NONE GMIME_COMPLIANCE_NONE

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
assert_parameters (GArray *issues, GMimeComplianceViolation expected, guint count)
{
	guint i, found = 0;

	for (i = 0; i < issues->len; i++) {
		GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

		if (issue->violation < BOUNDARY || issue->violation > PARAMETER)
			continue;
		g_assert_cmpint (issue->violation, ==, expected);
		g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_EXACT);
		found++;
	}
	g_assert_cmpuint (found, ==, count);
}

static void
test_repeated_parameters (void)
{
	/* MimeKit regressions inspired by https://github.com/noxxi/mime-is-broken. */
	static const struct {
		const char *field, *value;
		GMimeComplianceViolation violation;
		guint count;
	} cases[] = {
		{ "Content-Type", "multipart/mixed; boundary=a; BOUNDARY=b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=a; boundary=a; boundary=a", BOUNDARY, 2 },
		{ "Content-Type", "multipart/mixed; boundary=a; boundary*=''b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*=''a; boundary=b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*=''a; boundary*=''b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*0=a; boundary*1=b", NONE, 0 },
		{ "Content-Type", "multipart/mixed; boundary*1=b; boundary*0*=us-ascii''a", NONE, 0 },
		{ "Content-Type", "multipart/mixed; boundary*0=a; boundary*00*=b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=a; boundary*0=b; boundary*1=c", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*0=a; boundary*1=b; boundary=c", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*=''a; boundary*0=b; boundary*1=c", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary*0=a; boundary*1=b; boundary*=''c", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=a(comment; boundary=x); boundary (x) = b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=\"a\" boundary=b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=a(comment)boundary=b", BOUNDARY, 1 },
		{ "Content-Type", "multipart/mixed; boundary=a boundary=b", NONE, 0 },
		{ "Content-Type", "text/plain; name=file.png; NAME=file.zip", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename=file.png; filename=file.zip", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename*=''a; filename*=''b", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename=a; filename*=''b", NONE, 0 },
		{ "Content-Disposition", "attachment; filename*=''a; filename=b", NONE, 0 },
		{ "Content-Type", "text/plain; name=fallback; name*0*=utf-8''file; name*1=.zip", NONE, 0 },
		{ "Content-Disposition", "attachment; filename*0=a; filename*1=b; filename=fallback", NONE, 0 },
		{ "Content-Disposition", "attachment; filename=a; filename*=''b; filename=c", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename*=''a; filename=b; filename*=''c", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename=a; filename*0=b; filename*0*=c", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename*=''a; filename*0=b; filename*1=c", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename*0=a; filename*1=b; filename*=''c", FILENAME, 1 },
		{ "Content-Disposition", "attachment; filename * (x) 0 (x) * =''a; FILENAME*0=b", FILENAME, 1 },
		{ "Content-Type", "text/plain; charset=utf-8; CHARSET=us-ascii", PARAMETER, 1 },
		{ "Content-Type", "text/plain; charset=utf-8; charset*=us-ascii''utf-8", PARAMETER, 1 },
		{ "Content-Type", "text/plain; filename=a; filename=b", PARAMETER, 1 },
		{ "Content-Disposition", "attachment; name=a; name=b", PARAMETER, 1 },
		{ "Content-Disposition", "attachment; boundary=a; boundary=b", PARAMETER, 1 },
		{ "Content-Disposition", "attachment; filename=\"a; filename=b\"", NONE, 0 },
		{ "Content-Disposition", "attachment; filename=\"a\\\"; filename=b\"; size=1", NONE, 0 },
		{ "Content-Type", "text/plain; name=a (nested (name=x); name=y)", NONE, 0 },
		{ "Content-Type", "text/plain; name=a (escaped \\); name=x)", NONE, 0 },
		{ "Content-Type", "text/plain; name=\"a\\\"; name=x\"; name=b", FILENAME, 1 },
		{ "Content-Type", "text/plain;; broken; name=a; name=b", FILENAME, 1 },
		{ "Content-Type", "text/plain; name*999999999999999999=x; name=a", NONE, 0 },
		{ "Content-Type", "text/plain; name*2147483647=a; name*2147483647=b", FILENAME, 1 },
		{ "Content-Type", "text/plain; name*0x=a; name=a", NONE, 0 },
		{ "Content-Type", "text/plain; name=a; name=", FILENAME, 1 },
		{ "Content-Type", "text/plain; name=\"unterminated; name=b", NONE, 0 },
		{ "Content-Type", "text/plain; name=a (unterminated; name=b", NONE, 0 },
		{ "Content-Type", "text/plain", NONE, 0 },
		{ "X-Parameters", "text/plain; name=a; name=b", NONE, 0 },
		{ "Content-Type", "text/plain; x=1; x*0=2; x*1=3; x*1*=4", PARAMETER, 2 },
		{ "Content-Type", "text/plain; name*2147483648=a; name*2147483648=b", NONE, 0 },
		{ "Content-Type", "\"type; x=0\" (comment; x=0); x=1; x=2", PARAMETER, 1 }
	};
	guint i, chunked;

	testsuite_check ("Repeated raw MIME parameters, RFC 2231 representations and malformed declarations");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (chunked = 0; chunked < 2; chunked++) {
			char *wire = g_strdup_printf ("%s: %s\r\n\r\nbody\r\n", cases[i].field, cases[i].value);
			GArray *issues = read_issues (wire, chunked, NULL);

			assert_parameters (issues, cases[i].violation, cases[i].count);
			g_array_unref (issues);
			g_free (wire);
		}
	}
	testsuite_check_passed ();
}

static void
assert_position (const char *wire, const char *marker, GMimeComplianceIssue *issue,
		 GMimeCompliancePositionKind kind)
{
	const char *at = strstr (wire, marker), *p;
	guint64 line = 1, column = 1;

	g_assert_nonnull (at);
	for (p = wire; p < at; p++) {
		if (*p == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
	}
	g_assert_cmpint (issue->stream_offset, ==, at - wire);
	g_assert_cmpuint (issue->line_number, ==, line);
	g_assert_cmpuint (issue->column_number, ==, column);
	g_assert_cmpint (issue->position_kind, ==, kind);
}

static void
assert_continuations (GArray *issues, guint missing, guint repeated)
{
	guint i;

	g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, missing);
	for (i = 0; i < issues->len; i++) {
		GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

		if (issue->violation == MISSING_SEGMENT)
			g_assert_cmpint (issue->position_kind, ==, GMIME_COMPLIANCE_POSITION_ELEMENT_START);
	}
	assert_parameters (issues, FILENAME, repeated);
}

static void
test_continuations (void)
{
	/* MimeKit continuation-gap regressions inspired by mime-is-broken. */
	static const struct {
		const char *parameters;
		guint missing, repeated;
	} cases[] = {
		{ "filename*0=a", 0, 0 },
		{ "filename*0=a; filename*1=b", 0, 0 },
		{ "filename*1=b; filename*0=a", 0, 0 },
		{ "filename*2=c; filename*0=a; filename*1=b", 0, 0 },
		{ "filename*1=a; filename*2=b", 1, 0 },
		{ "filename*0=a; filename*2=b", 1, 0 },
		{ "filename*0=a; filename*0=b; filename*1=c", 0, 1 },
		{ "filename*1=a; filename*1=b; filename*3=c", 1, 1 },
		{ "filename*0=a; filename*2147483647=b", 1, 0 },
		{ "filename*2147483647=a; filename*2147483647=b", 1, 1 },
		{ "filename*0000=a; filename*0001=b", 0, 0 },
		{ "filename=fallback; filename*1=b", 1, 0 },
		{ "filename*1=b; filename=fallback", 1, 0 },
		{ "filename*=us-ascii''fallback; filename*1=b", 1, 1 },
		{ "filename*1=b; filename*=us-ascii''fallback", 1, 1 },
		{ "filename*1*=b; filename*0*=us-ascii''a", 0, 0 },
		{ "filename*0*=us-ascii''a; filename*2=b", 1, 0 },
		{ "filename*0=a; filename*0*=b", 0, 1 },
		{ "filename*2147483648=a; filename*1=b", 1, 0 },
		{ "filename*9999999999999999999999=a; filename*0=b", 0, 0 },
		{ "filename*=us-ascii''a", 0, 0 },
		{ "filename*0=a; other*1=b; filename*2=c", 2, 0 },
		{ "filename*2=a; other*1=b; filename*0=c; other*0=d", 1, 0 },
		{ "boundary*0=a; boundary*2=b", 1, 0 },
		{ "filename*1=", 1, 0 },
		{ "filename*1=\"unterminated; filename*0=a", 1, 0 },
		{ "filename*1=a (unterminated; filename*0=b", 1, 0 }
	};
	guint i, content_type, chunked;

	testsuite_check ("Missing continuation segments in either header, independent of order and fallback");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (content_type = 0; content_type < 2; content_type++) {
			GString *parameters = g_string_new (cases[i].parameters);
			char *wire;

			if (content_type)
				g_string_replace (parameters, "filename", "name", 0);
			wire = g_strdup_printf ("%s; %s\r\n\r\nbody\r\n", content_type ?
				"Content-Type: text/plain" : "Content-Disposition: attachment", parameters->str);
			for (chunked = 0; chunked < 2; chunked++) {
				GArray *issues = read_issues (wire, chunked, NULL);

				assert_continuations (issues, cases[i].missing, cases[i].repeated);
				g_array_unref (issues);
			}
			g_string_free (parameters, TRUE);
			g_free (wire);
		}
	}
	testsuite_check_passed ();
}

static void
test_positions_and_gating (void)
{
	const char *wire = "X-Prefix: \303\251\r\nContent-Type \t: text/plain; charset=utf-8;\r\n"
		"\t(comment; charset=x)\r\n\t(\303\251) CHARSET=us-ascii; charset=iso-8859-1\r\n"
		"Content-Type: text/plain; name=a; name=b\r\n"
		"Content-Disposition: attachment; filename=a; filename=b\r\n"
		"Content-Disposition: inline; filename=c; filename=d\r\n\r\nbody\r\n";
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint chunked, enabled, i, count;

	testsuite_check ("Parameter positions include raw folds and UTF-8; ignored headers are validated");
	for (chunked = 0; chunked < 2; chunked++) {
		for (enabled = 0; enabled < 3; enabled++) {
			GArray *issues;

			g_mime_compliance_options_set_enabled_validators (options, enabled == 0 ?
				GMIME_COMPLIANCE_VALIDATOR_NONE : enabled == 1 ?
				GMIME_COMPLIANCE_VALIDATOR_PARAMETERS : GMIME_COMPLIANCE_VALIDATOR_ALL);
			issues = read_issues (wire, chunked, options);
			g_assert_cmpuint (count_issues (issues, PARAMETER), ==, enabled ? 2 : 0);
			g_assert_cmpuint (count_issues (issues, FILENAME), ==, enabled ? 3 : 0);
			g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, 1);
			for (i = 0, count = 0; i < issues->len; i++) {
				GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

				if (issue->violation == PARAMETER)
					assert_position (wire, count++ ? "charset=iso" : "CHARSET=us", issue,
							 GMIME_COMPLIANCE_POSITION_EXACT);
				else if (issue->violation == DISPOSITION)
					assert_position (wire, "Content-Disposition: inline", issue,
							 GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			}
			g_array_unref (issues);
		}
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_continuation_positions_and_gating (void)
{
	const char *folded = "Content-Type: text/plain; z*0=a;\r\n\tname=fallback;\r\n"
		"\t(\303\251) name*2=b; z*0=c;\r\n\tother*1=x; name*4=d; other*3=y\r\n\r\nbody\r\n";
	const char *separate = "Content-Type: text/plain; name*0=a\r\n"
		"Content-Type: text/plain; name*1=b\r\n"
		"Content-Disposition: attachment; filename*0=a\r\n"
		"Content-Disposition: attachment; filename*1=b\r\n\r\nbody\r\n";
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint chunked, enabled, i, found;

	testsuite_check ("Gap positions survive later diagnostics; segments do not cross header boundaries");
	for (chunked = 0; chunked < 2; chunked++) {
		GArray *issues = read_issues (folded, chunked, NULL);

		g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 2);
		g_assert_cmpuint (count_issues (issues, PARAMETER), ==, 1);
		for (i = 0, found = 0; i < issues->len; i++) {
			GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

			if (issue->violation == MISSING_SEGMENT)
				assert_position (folded, found++ ? "other*1=" : "name*2=", issue,
						 GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			else if (issue->violation == PARAMETER)
				assert_position (folded, "z*0=c", issue, GMIME_COMPLIANCE_POSITION_EXACT);
		}
		g_array_unref (issues);
		for (enabled = 0; enabled < 3; enabled++) {
			g_mime_compliance_options_set_enabled_validators (options, enabled == 0 ?
				GMIME_COMPLIANCE_VALIDATOR_NONE : enabled == 1 ?
				GMIME_COMPLIANCE_VALIDATOR_PARAMETERS : GMIME_COMPLIANCE_VALIDATOR_ALL);
			issues = read_issues (separate, chunked, options);
			assert_continuations (issues, enabled ? 2 : 0, 0);
			g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, 1);
			g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_REPEATED_CONTENT_TYPE), ==, 1);
			for (i = 0, found = 0; i < issues->len; i++) {
				GMimeComplianceIssue *issue = &g_array_index (issues, GMimeComplianceIssue, i);

				if (issue->violation == MISSING_SEGMENT)
					assert_position (separate, found++ ? "filename*1=b" : "name*1=b", issue,
							 GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			}
			g_array_unref (issues);
		}
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_resets_and_budgets (void)
{
	const char *nested = "Content-Type: multipart/mixed; boundary=b\r\nContent-Disposition: inline\r\n\r\n"
		"--b\r\nContent-Type: text/plain; name*0=a\r\nContent-Disposition: inline; filename*0=a\r\n\r\none\r\n"
		"--b\r\nContent-Type: message/rfc822\r\nContent-Disposition: inline\r\n\r\n"
		"Content-Type: text/plain; name*0=a\r\nContent-Disposition: inline; filename*0=a\r\n"
		"Content-Disposition: attachment; filename=a\r\n\r\ntwo\r\n--b--\r\n";
	const char *mbox = "From sender\nContent-Type: text/plain; name*0=a\nContent-Disposition: inline\n\none\n"
		"From sender\nContent-Type: text/plain; name*0=a\nContent-Disposition: inline\n\n two\n";
	const char *budget = "Content-Type: text/plain; boundary=a; boundary=b; boundary=c;\r\n"
		" name=a; name=b; name=c; x=1; x=2; x=3\r\n"
		"Content-Disposition: inline; a=fallback; b*3=y; a*1=x\r\nContent-Disposition: inline\r\n"
		"Content-Disposition: attachment\r\n\r\nbody\r\n";
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint chunked, i, operation;

	testsuite_check ("Per-header state resets for entities and mbox messages; independent diagnostic budgets");
	for (chunked = 0; chunked < 2; chunked++) {
		GArray *issues = read_issues (nested, chunked, NULL);
		GMimeStream *stream = new_input (mbox, chunked);
		GMimeReader *reader = g_mime_reader_new_with_format (NULL, stream, GMIME_FORMAT_MBOX);
		GError *error = NULL;

		assert_parameters (issues, NONE, 0);
		g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 0);
		g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, 1);
		g_array_set_size (issues, 0);
		g_mime_reader_set_compliance_logger (reader, collect_issue, issues, NULL);
		for (i = 0; i < 2; i++) {
			g_assert_true (g_mime_reader_read_message (reader, &error));
			g_assert_no_error (error);
		}
		assert_parameters (issues, NONE, 0);
		g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 0);
		g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, 0);
		g_object_unref (reader);
		g_object_unref (stream);
		g_array_unref (issues);

		g_mime_compliance_options_set_context (options, GMIME_COMPLIANCE_CONTEXT_STORAGE);
		g_mime_compliance_options_set_max_issues_per_violation (options, 1);
		for (operation = 0; operation < 3; operation++) {
			issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
			stream = new_input (budget, chunked);
			reader = g_mime_reader_new (NULL, stream);
			g_mime_reader_set_compliance_options (reader, options);
			g_mime_reader_set_compliance_logger (reader, collect_issue, issues, NULL);
			if (operation == 0)
				g_assert_true (g_mime_reader_read_headers (reader, &error));
			else if (operation == 1)
				g_assert_true (g_mime_reader_read_entity (reader, &error));
			else
				g_assert_true (g_mime_reader_read_message (reader, &error));
			g_assert_no_error (error);
			g_assert_cmpuint (count_issues (issues, BOUNDARY), ==, 1);
			g_assert_cmpuint (count_issues (issues, FILENAME), ==, 1);
			g_assert_cmpuint (count_issues (issues, PARAMETER), ==, 1);
			g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, 1);
			g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 1);
			g_assert_cmpuint (count_issues (issues, GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES), ==, 1);
			for (i = 0; i < issues->len; i++) {
				g_assert_cmpint (g_array_index (issues, GMimeComplianceIssue, i).context,
						 ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
				if (g_array_index (issues, GMimeComplianceIssue, i).violation == MISSING_SEGMENT)
					assert_position (budget, "a*1=x", &g_array_index (issues, GMimeComplianceIssue, i),
							 GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			}
			g_object_unref (reader);
			g_object_unref (stream);
			g_array_unref (issues);
		}
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_bounded_validation (void)
{
	const char slice[] = "xxtext/plain; x=1;\r\n x=2; x=3yy";
	const char embedded[] = "text/plain; x=1; bad=\0; x=2";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GMimeComplianceIssue *issue;
	GString *nested;
	guint8 *buffer;
	gsize length;
	guint i, repeat;

	testsuite_check ("Bounded raw slices, embedded NULs, exact coordinates and independent validator calls");
	length = sizeof (slice) - 5;
	for (repeat = 0; repeat < 2; repeat++) {
		buffer = g_memdup2 (slice + 2, length);
		_g_mime_parameter_validate (buffer, length, TRUE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_STORAGE, 100, 5, 7);
		g_assert_cmpuint (issues->len, ==, 2);
		for (i = 0; i < 2; i++) {
			issue = &g_array_index (issues, GMimeComplianceIssue, i);
			g_assert_cmpint (issue->stream_offset, ==, 100 + (strstr (slice, i ? "x=3" : "x=2") - (slice + 2)));
			g_assert_cmpuint (issue->line_number, ==, 6);
			g_assert_cmpuint (issue->column_number, ==, i ? 7 : 2);
		}
		g_free (buffer);
		g_array_set_size (issues, 0);
	}
	for (length = 0; length < sizeof (embedded); length++) {
		buffer = g_memdup2 (embedded, length);
		_g_mime_parameter_validate (buffer, length, TRUE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
		assert_parameters (issues, PARAMETER, length >= sizeof (embedded) - 2 ? 1 : 0);
		g_array_set_size (issues, 0);
		g_free (buffer);
	}
	nested = g_string_new ("text/plain; x=1 ");
	for (i = 0; i < 16384; i++)
		g_string_append_c (nested, '(');
	g_string_append (nested, "; x=ignored");
	for (i = 0; i < 16384; i++)
		g_string_append_c (nested, ')');
	g_string_append (nested, "; x=2");
	_g_mime_parameter_validate ((const guint8 *) nested->str, nested->len, TRUE, collect_issue, issues,
		GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
	assert_parameters (issues, PARAMETER, 1);
	g_string_free (nested, TRUE);
	g_array_unref (issues);
	testsuite_check_passed ();
}

static void
test_continuation_bounds (void)
{
	const char slice[] = "xxtext/plain; name*2=x;\r\n name*2=yyy";
	const char complete[] = "text/plain; name*1=b; name*0=a";
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	gsize length = sizeof (slice) - 5;
	guint8 *buffer = g_memdup2 (slice + 2, length);
	guint repeat, sparse, i;

	testsuite_check ("Continuation snapshots reset between bounded calls; tracking depends on declaration count");
	for (repeat = 0; repeat < 2; repeat++) {
		GMimeComplianceIssue *missing, *duplicate;

		_g_mime_parameter_validate (buffer, length, TRUE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_STORAGE, 100, 5, 7);
		assert_continuations (issues, 1, 1);
		g_assert_cmpuint (issues->len, ==, 2);
		duplicate = &g_array_index (issues, GMimeComplianceIssue, 0);
		missing = &g_array_index (issues, GMimeComplianceIssue, 1);
		g_assert_cmpint (duplicate->violation, ==, FILENAME);
		g_assert_cmpuint (duplicate->line_number, ==, 6);
		g_assert_cmpuint (duplicate->column_number, ==, 2);
		g_assert_cmpint (missing->violation, ==, MISSING_SEGMENT);
		g_assert_cmpint (missing->stream_offset, ==, 100 + (strstr (slice, "name*2") - (slice + 2)));
		g_assert_cmpuint (missing->line_number, ==, 5);
		g_assert_cmpuint (missing->column_number, ==, 7 + (strstr (slice, "name*2") - (slice + 2)));
		g_assert_cmpint (missing->context, ==, GMIME_COMPLIANCE_CONTEXT_STORAGE);
		g_array_set_size (issues, 0);
		_g_mime_parameter_validate ((const guint8 *) complete, sizeof (complete) - 1, TRUE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_STORAGE, 100, 5, 7);
		g_assert_cmpuint (issues->len, ==, 0);
	}
	g_free (buffer);
	for (sparse = 0; sparse < 2; sparse++) {
		GString *text = g_string_new ("attachment");
		gint64 start;
		int last = 0;

		for (i = 0; i < 4096; i++) {
			last = sparse ? G_MAXINT32 - (int) i * 9059 : 4095 - (int) i;
			g_string_append_printf (text, "; filename*%d=x", last);
		}
		g_string_append_printf (text, "; filename*%d=y", last);
		start = g_get_monotonic_time ();
		_g_mime_parameter_validate ((const guint8 *) text->str, text->len, FALSE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
		g_assert_cmpint (g_get_monotonic_time () - start, <, 5 * G_USEC_PER_SEC);
		assert_continuations (issues, sparse ? 1 : 0, 1);
		g_array_set_size (issues, 0);
		g_string_free (text, TRUE);
	}
	{
		GString *text = g_string_new ("text/plain; name*0=0");
		gint64 start;

		for (i = 0; i < 16384; i++)
			g_string_append (text, ";\r\n name*0=1");
		start = g_get_monotonic_time ();
		_g_mime_parameter_validate ((const guint8 *) text->str, text->len, TRUE, collect_issue, issues,
			GMIME_COMPLIANCE_CONTEXT_TRANSPORT, 0, 1, 1);
		g_assert_cmpint (g_get_monotonic_time () - start, <, 5 * G_USEC_PER_SEC);
		assert_continuations (issues, 0, 16384);
		g_assert_cmpuint (g_array_index (issues, GMimeComplianceIssue, issues->len - 1).line_number, ==, 16385);
		g_string_free (text, TRUE);
	}
	g_array_unref (issues);
	testsuite_check_passed ();
}

static void
test_header_limits_and_linearity (void)
{
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
	GString *wire = g_string_new ("Content-Type: text/plain; name*1=a; x=0");
	GMimeStream *stream;
	GMimeReader *reader;
	GMimeComplianceIssue *last = NULL;
	GError *error = NULL;
	gint64 start;
	guint i, limit;

	testsuite_check ("Header limits precede validation; 16384 folded repetitions scan in bounded time");
	for (i = 0; i < 16384; i++)
		g_string_append (wire, ";\r\n x=1");
	g_string_append (wire, "\r\n\r\nbody\r\n");
	for (limit = 0; limit < 2; limit++) {
		g_mime_parser_options_set_max_header_length (options, limit ? 128 : wire->len);
		stream = new_input (wire->str, limit != 0);
		reader = g_mime_reader_new (options, stream);
		g_mime_reader_set_compliance_logger (reader, collect_issue, issues, NULL);
		start = g_get_monotonic_time ();
		if (limit) {
			g_assert_false (g_mime_reader_read_message (reader, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
			assert_parameters (issues, NONE, 0);
			g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 0);
		} else {
			g_assert_true (g_mime_reader_read_message (reader, &error));
			g_assert_no_error (error);
			g_assert_cmpint (g_get_monotonic_time () - start, <, 5 * G_USEC_PER_SEC);
			assert_parameters (issues, PARAMETER, 16384);
			g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, 1);
			for (i = 0; i < issues->len; i++) {
				if (g_array_index (issues, GMimeComplianceIssue, i).violation == PARAMETER)
					last = &g_array_index (issues, GMimeComplianceIssue, i);
			}
			g_assert_nonnull (last);
			g_assert_cmpuint (last->line_number, ==, 16385);
			g_assert_cmpuint (last->column_number, ==, 2);
		}
		g_array_set_size (issues, 0);
		g_object_unref (reader);
		g_object_unref (stream);
	}
	g_string_free (wire, TRUE);
	g_array_unref (issues);
	g_mime_parser_options_unref (options);
	testsuite_check_passed ();
}

static void
test_model_preservation (gboolean continued)
{
	const char *wire = continued ?
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Disposition: attachment; filename*0=file.; filename*2=txt\r\n\r\nbody\r\n" :
		"Content-Type: text/plain; charset=utf-8; charset=us-ascii\r\n"
		"Content-Type: application/octet-stream; name=a; name=b\r\n"
		"Content-Disposition: attachment; filename=a; filename=b\r\n"
		"Content-Disposition: inline; filename=c; filename=d\r\n\r\nbody\r\n";
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();
	guint chunked, mode;

	testsuite_check ("Parameter diagnostics do not change header precedence, filenames or serialized models");
	for (chunked = 0; chunked < 2; chunked++) {
		GByteArray *baseline = NULL;

		for (mode = 0; mode < 3; mode++) {
			GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue));
			GMimeStream *input = new_input (wire, chunked), *output = g_mime_stream_mem_new ();
			GMimeParser *parser = g_mime_parser_new_with_stream (input);
			GMimeMessage *message;
			GMimeEntity *part;
			GByteArray *bytes;
			char *text;
			GError *error = NULL;

			g_mime_compliance_options_set_enabled_validators (options, mode == 1 ?
				GMIME_COMPLIANCE_VALIDATOR_NONE : GMIME_COMPLIANCE_VALIDATOR_ALL);
			g_mime_parser_set_compliance_options (parser, options);
			if (mode)
				g_mime_parser_set_compliance_logger (parser, collect_issue, issues, NULL);
			message = g_mime_parser_construct_message (parser, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (message);
			part = g_mime_message_get_mime_part (message);
			g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (part), "text", "plain"));
			g_assert_null (g_mime_entity_get_content_type_parameter (part, "name"));
			g_assert_cmpstr (g_mime_entity_get_content_type_parameter (part, "charset"), ==, "utf-8");
			g_assert_cmpstr (g_mime_part_get_filename (GMIME_PART (part)), ==, continued ? "file.txt" : "a");
			g_assert_true (GMIME_IS_TEXT_PART (part));
			text = g_mime_text_part_get_text (GMIME_TEXT_PART (part));
			g_assert_cmpstr (text, ==, "body\r\n");
			g_free (text);
			g_assert_cmpint (g_mime_entity_write_to_stream (GMIME_ENTITY (message), NULL, output), >, 0);
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
			if (mode)
				g_assert_cmpmem (bytes->data, bytes->len, baseline->data, baseline->len);
			else
				baseline = g_byte_array_ref (bytes);
			g_assert_cmpuint (count_issues (issues, PARAMETER), ==, mode == 2 && !continued ? 1 : 0);
			g_assert_cmpuint (count_issues (issues, FILENAME), ==, mode == 2 && !continued ? 3 : 0);
			g_assert_cmpuint (count_issues (issues, DISPOSITION), ==, mode && !continued ? 1 : 0);
			g_assert_cmpuint (count_issues (issues, MISSING_SEGMENT), ==, mode == 2 && continued ? 1 : 0);
			g_array_unref (issues);
			g_object_unref (message);
			g_object_unref (parser);
			g_object_unref (input);
			g_object_unref (output);
		}
		g_byte_array_unref (baseline);
	}
	g_mime_compliance_options_unref (options);
	testsuite_check_passed ();
}

static void
test_metadata (void)
{
	static const struct {
		GMimeComplianceViolation violation;
		guint value;
		const char *name;
	} cases[] = {
		{ DISPOSITION, 13, "RepeatedContentDisposition" },
		{ BOUNDARY, 44, "RepeatedBoundaryParameter" },
		{ FILENAME, 45, "RepeatedFilenameParameter" },
		{ PARAMETER, 46, "RepeatedParameter" },
		{ MISSING_SEGMENT, 47, "MissingParameterSegment" }
	};
	guint i, context;

	testsuite_check ("MimeKit diagnostic identities, flag values and metadata");
	g_assert_cmpuint (GMIME_COMPLIANCE_VALIDATOR_PARAMETERS, ==, 16);
	g_assert_cmpuint (GMIME_COMPLIANCE_VALIDATOR_ALL, ==, 31);
	g_assert_cmpuint (GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT, ==, 95);
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_assert_cmpuint (cases[i].violation, ==, cases[i].value);
		g_assert_cmpstr (g_mime_compliance_violation_get_name (cases[i].violation), ==, cases[i].name);
		g_assert_cmpint (g_mime_compliance_violation_get_categories (cases[i].violation), ==,
				GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_SECURITY);
		for (context = GMIME_COMPLIANCE_CONTEXT_TRANSPORT; context <= GMIME_COMPLIANCE_CONTEXT_STORAGE; context++)
			g_assert_cmpint (g_mime_compliance_violation_get_severity (cases[i].violation, context), ==,
				cases[i].violation == BOUNDARY ? GMIME_COMPLIANCE_SEVERITY_CRITICAL : GMIME_COMPLIANCE_SEVERITY_MAJOR);
	}
	testsuite_check_passed ();
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME parameter compliance");
	test_repeated_parameters ();
	test_continuations ();
	test_positions_and_gating ();
	test_continuation_positions_and_gating ();
	test_resets_and_budgets ();
	test_bounded_validation ();
	test_continuation_bounds ();
	test_header_limits_and_linearity ();
	test_model_preservation (FALSE);
	test_model_preservation (TRUE);
	test_metadata ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
