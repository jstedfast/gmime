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

#include <gmime/gmime-encoding-validator-private.h>
#include <gmime/gmime-codec-private.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct {
	GMimeEncodingIssue issues[8];
	guint length;
} IssueLog;

typedef struct {
	const char *input;
	guint count;
	GMimeEncodingIssue issues[4];
} ValidatorCase;

static void
record_issue (const GMimeEncodingIssue *issue, gpointer user_data)
{
	IssueLog *log = user_data;

	g_assert_cmpuint (log->length, <, G_N_ELEMENTS (log->issues));
	log->issues[log->length++] = *issue;
}

static GMimeEncodingValidator *
new_validator (guint kind, IssueLog *log, gint64 offset, guint64 line)
{
	if (kind == 0)
		return _g_mime_base64_validator_new (record_issue, log, offset, line);
	if (kind == 1)
		return _g_mime_quoted_printable_validator_new (record_issue, log, offset, line);

	return _g_mime_uu_validator_new (record_issue, log, offset, line);
}

static void
assert_issue (const GMimeEncodingIssue *actual, const GMimeEncodingIssue *expected,
	      gint64 initial_offset, guint64 initial_line)
{
	g_assert_cmpint (actual->violation, ==, expected->violation);
	g_assert_cmpint (actual->stream_offset, ==, expected->stream_offset + initial_offset);
	g_assert_cmpuint (actual->line_number, ==, expected->line_number + initial_line - 1);
	g_assert_cmpuint (actual->column_number, ==, expected->column_number);
}

static void
check_input (guint kind, const guint8 *input, gsize length,
	     const GMimeEncodingIssue *issues, guint count)
{
	static const GMimeContentEncoding encodings[] = {
		GMIME_CONTENT_ENCODING_BASE64, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, GMIME_CONTENT_ENCODING_UUENCODE
	};
	gsize split, i;
	guint j;

	for (split = 0; split <= length + 1; split++) {
		IssueLog log = { 0 };
		GMimeEncodingValidator *validator = new_validator (kind, &log, 100, 5);
		gpointer weak = validator;

		g_object_add_weak_pointer (G_OBJECT (validator), &weak);
		g_assert_cmpint (_g_mime_encoding_validator_get_encoding (validator), ==, encodings[kind]);
		if (split <= length) {
			_g_mime_encoding_validator_write (validator, input, split);
			_g_mime_encoding_validator_write (validator, NULL, 0);
			_g_mime_encoding_validator_write (validator, input + split, length - split);
		} else {
			for (i = 0; i < length; i++)
				_g_mime_encoding_validator_write (validator, input + i, 1);
		}
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, count);
		for (j = 0; j < log.length; j++)
			assert_issue (&log.issues[j], &issues[j], 100, 5);
		g_object_unref (validator);
		g_assert_null (weak);
	}
}

static void
check_case (guint kind, const ValidatorCase *test)
{
	check_input (kind, (const guint8 *) test->input, strlen (test->input), test->issues, test->count);
}

static void
test_base64 (void)
{
	static const ValidatorCase cases[] = {
		{ "", 0, {{0}} },
		{ "TWFu\r\nTQ== \t", 0, {{0}} },
		{ "TQ=\r\n=", 0, {{0}} },
		{ "??**\n?!*", 4, {
			{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 0, 1, 1 },
			{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 2, 1, 3 },
			{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 5, 2, 1 },
			{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 7, 2, 3 }
		} },
		{ "=", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING, 0, 1, 1 }} },
		{ "A=", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING, 1, 1, 2 }} },
		{ "AA===", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING, 4, 1, 5 }} },
		{ "AA==A", 1, {{ GMIME_ENCODING_VIOLATION_BASE64_CHARACTERS_AFTER_PADDING, 4, 1, 5 }} },
		{ "AA==\r\n*", 1, {{ GMIME_ENCODING_VIOLATION_BASE64_CHARACTERS_AFTER_PADDING, 6, 2, 1 }} },
		{ "AAA", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM, 3, 1, 4 }} },
		{ "AA=\n", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM, 4, 2, 1 }} },
		{ "TWFu*\n?", 2, {
			{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 4, 1, 5 },
			{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 6, 2, 1 }
		} },
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		check_case (0, &cases[i]);
}

static void
test_quoted_printable (void)
{
	static const ValidatorCase cases[] = {
		{ "", 0, {{0}} },
		{ "ascii_=C3=A9=\r\ntext=\nmore", 0, {{0}} },
		{ "=XA=~", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 1, 1, 2 }} },
		{ "=AX", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 2, 1, 3 }} },
		{ "=A\n=~", 2, {
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 2, 1, 3 },
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 4, 2, 2 }
		} },
		{ "=\rX=\rY", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK, 2, 1, 3 }} },
		{ "=~=\r~\n=~", 3, {
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 1, 1, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK, 4, 1, 5 },
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 7, 2, 2 }
		} },
		{ "==41", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 1, 1, 2 }} },
		{ "=\r=\r\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK, 2, 1, 3 }} },
		{ "=", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 1, 1, 2 }} },
		{ "=A", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 2, 1, 3 }} },
		{ "=\r", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK, 2, 1, 3 }} },
		{ "=~=", 2, {
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 1, 1, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 3, 1, 4 }
		} },
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		check_case (1, &cases[i]);
}

static void
test_uuencode (void)
{
	static const ValidatorCase cases[] = {
		{ "", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 0, 1, 1 }} },
		{ "begin 644 x\n`\nend\n", 0, {{0}} },
		{ "begin 0644 x\r\n#ABCD\r\n`\t \r\nend\r\n \t", 0, {{0}} },
		{ "\t ignored\r\nbegin 999 x\n \n`\nend", 0, {{0}} },
		{ "x\nbegin 644 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT, 0, 1, 1 }} },
		{ "bx\nbegin 644 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT, 1, 1, 2 }} },
		{ "be\nbegin 644 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT, 2, 2, 0 }} },
		{ "begin 64 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, 8, 1, 9 }} },
		{ "begin 06440 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, 10, 1, 11 }} },
		{ "begin x644 x\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, 6, 1, 7 }} },
		{ "begin 644\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, 9, 1, 10 }} },
		{ "begin 644 x\nNABCD\n`\nend\n", 2, {
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_LENGTH, 12, 2, 1 },
			{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE, 17, 2, 6 }
		} },
		{ "begin 644 x\n#AB\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE, 15, 2, 4 }} },
		{ "begin 644 x\n#AB\n#CD\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE, 15, 2, 4 }} },
		{ "begin 644 x\n#AB\n\n`\nend\n", 2, {
			{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE, 15, 2, 4 },
			{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE, 16, 3, 1 }
		} },
		{ "begin 644 x\n#A\rBCD\n`\nend\n", 0, {{0}} },
		{ "begin 644 x\n#abc~\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT, 13, 2, 2 }} },
		{ "begin 644 x\n#ABCDx~\n`\nend\n", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_EXTRA_DATA, 17, 2, 6 }} },
		{ "begin 644 x\n#aaaaXXXX\n#bbbbYYYY\n`\nend\n", 4, {
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT, 13, 2, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_EXTRA_DATA, 17, 2, 6 },
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT, 23, 3, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_EXTRA_DATA, 27, 3, 6 }
		} },
		{ "begin 644 x\n`xy\nend\n", 2, {
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 13, 2, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 14, 2, 3 }
		} },
		{ "begin 644 x\n`\v\f\nend\n", 2, {
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 13, 2, 2 },
			{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 14, 2, 3 }
		} },
		{ "begin 644 x\n`\nxnd\nignored", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 14, 3, 1 }} },
		{ "begin 644 x\n`\nexd", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 15, 3, 2 }} },
		{ "begin 644 x\n`\nenx", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 16, 3, 3 }} },
		{ "begin 644 x\n`\nendx", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 17, 3, 4 }} },
		{ "begin 644 x\n`\nend\nmore", 1, {{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER, 18, 4, 1 }} },
		{ "begin 644 x\n", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 12, 2, 1 }} },
		{ "begin 644 x\n`", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 13, 2, 2 }} },
		{ "begin 644 x\n`\n", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 14, 3, 1 }} },
		{ "begin 644 x\n`\ne", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 15, 3, 2 }} },
		{ "begin 644 x\n`\nen", 1, {{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 16, 3, 3 }} },
		{ "begin 644 x\n`\nend", 0, {{0}} },
	};
	static const guint8 pretext[] = "x\0begin 644 x\n`\nend\n";
	static const GMimeEncodingIssue pretext_issues[] = {
		{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT, 0, 1, 1 },
		{ GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, 20, 4, 1 }
	};
	static const guint8 payload[] = "begin 644 x\n#\0\0\0\0\n`\nend\n";
	static const GMimeEncodingIssue payload_issues[] = {
		{ GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT, 13, 2, 2 }
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		check_case (2, &cases[i]);
	check_input (2, pretext, sizeof pretext - 1, pretext_issues, G_N_ELEMENTS (pretext_issues));
	check_input (2, payload, sizeof payload - 1, payload_issues, G_N_ELEMENTS (payload_issues));
}

static void
test_uu_file_mode (void)
{
	static const guint lengths[] = { 255, 256, 259, 260 };
	static const GMimeEncodingIssue issue = {
		GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, 10, 1, 11
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (lengths); i++) {
		char *mode = g_strnfill (lengths[i], '6');
		char *input = g_strconcat ("begin ", mode, " x\n`\nend\n", NULL);

		check_input (2, (const guint8 *) input, strlen (input), &issue, 1);
		g_free (input);
		g_free (mode);
	}
}

static void
test_uu_flush (void)
{
	static const guint8 input[] = "begin 644 x\n#ABCD\n`\nend\n";
	gsize length, i;

	for (length = 0; length < sizeof input; length++) {
		IssueLog log = { 0 };
		GMimeEncodingValidator *validator = new_validator (2, &log, 100, 5);
		GMimeEncodingIssue expected = {
			GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT, (gint64) length, 1, 1
		};
		guint count = length < sizeof input - 2 ? 2 : 0;

		for (i = 0; i < length; i++) {
			if (input[i] == '\n') {
				expected.line_number++;
				expected.column_number = 1;
			} else {
				expected.column_number++;
			}
		}
		_g_mime_encoding_validator_write (validator, input, length);
		_g_mime_encoding_validator_flush (validator);
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, count);
		for (i = 0; i < log.length; i++)
			assert_issue (&log.issues[i], &expected, 100, 5);
		_g_mime_encoding_validator_write (validator, NULL, 0);
		_g_mime_encoding_validator_write (validator, input + length, sizeof input - 1 - length);
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, count);
		g_object_unref (validator);
	}
}

static void
test_flush (void)
{
	static const char *inputs[] = { "A", "=", "b" };
	static const char *suffixes[] = { "AAA", "41", "egin 644 x\n`\nend\n" };
	guint kind;

	for (kind = 0; kind < 3; kind++) {
		IssueLog log = { 0 };
		GMimeEncodingValidator *validator = new_validator (kind, &log, 0, 1);
		const char *input = inputs[kind];
		const char *rest = suffixes[kind];

		_g_mime_encoding_validator_write (validator, (const guint8 *) input, 1);
		_g_mime_encoding_validator_flush (validator);
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, 2);
		assert_issue (&log.issues[1], &log.issues[0], 0, 1);
		_g_mime_encoding_validator_write (validator, (const guint8 *) rest, strlen (rest));
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, 2);
		g_object_unref (validator);
	}
}

static void
test_preconditions (void)
{
	static GMimeEncodingValidator *(*const constructors[]) (GMimeEncodingIssueFunc, gpointer, gint64, guint64) = {
		_g_mime_base64_validator_new, _g_mime_quoted_printable_validator_new, _g_mime_uu_validator_new
	};
	static const GMimeEncodingViolation violations[] = {
		GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM,
		GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING,
		GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT
	};
	static const char *inputs[] = { "\nA", "\n=", "\n " };
	guint kind;

	g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*GMIME_IS_ENCODING_VALIDATOR*failed*");
	g_assert_cmpint (_g_mime_encoding_validator_get_encoding (NULL), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_test_assert_expected_messages ();
	g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*GMIME_IS_ENCODING_VALIDATOR*failed*");
	_g_mime_encoding_validator_write (NULL, NULL, 0);
	g_test_assert_expected_messages ();
	g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*GMIME_IS_ENCODING_VALIDATOR*failed*");
	_g_mime_encoding_validator_flush (NULL);
	g_test_assert_expected_messages ();

	for (kind = 0; kind < G_N_ELEMENTS (constructors); kind++) {
		GMimeEncodingValidator *(*create) (GMimeEncodingIssueFunc, gpointer, gint64, guint64) =
			constructors[kind];
		IssueLog log = { 0 };
		GMimeEncodingValidator *validator;
		GMimeEncodingIssue expected = {
			violations[kind],
			G_MAXINT64, (guint64) G_MAXUINT + 2, 2
		};

		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*report != NULL*failed*");
		g_assert_null (create (NULL, NULL, 0, 1));
		g_test_assert_expected_messages ();
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*stream_offset >= 0*failed*");
		g_assert_null (create (record_issue, &log, -1, 1));
		g_test_assert_expected_messages ();
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*line_number > 0*failed*");
		g_assert_null (create (record_issue, &log, 0, 0));
		g_test_assert_expected_messages ();
		validator = create (record_issue, &log, G_MAXINT64 - 2, (guint64) G_MAXUINT + 1);
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*input != NULL*failed*");
		_g_mime_encoding_validator_write (validator, NULL, 1);
		g_test_assert_expected_messages ();
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*length*failed*");
		_g_mime_encoding_validator_write (validator, (const guint8 *) "A", G_MAXSIZE);
		g_test_assert_expected_messages ();
		_g_mime_encoding_validator_write (validator, (const guint8 *) inputs[kind], 2);
		_g_mime_encoding_validator_write (validator, NULL, 0);
		g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*assertion*length*failed*");
		_g_mime_encoding_validator_write (validator, (const guint8 *) "A", 1);
		g_test_assert_expected_messages ();
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, 1);
		assert_issue (&log.issues[0], &expected, 0, 1);
		g_object_unref (validator);
	}
}

static void
test_uu_guard_pages (void)
{
#ifdef G_OS_UNIX
	static const guint8 input[] = "begin 644 x\r\n#ABCD\r\n`\r\nend\r\n";
	gsize page = (gsize) sysconf (_SC_PAGESIZE);
	guint8 *map = mmap (NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	gsize length;

	g_assert_true (map != MAP_FAILED);
	g_assert_cmpint (mprotect (map + page, page, PROT_NONE), ==, 0);
	for (length = 0; length < sizeof input; length++) {
		IssueLog log = { 0 };
		GMimeEncodingValidator *validator = new_validator (2, &log, 0, 1);

		g_assert_cmpint (mprotect (map, page, PROT_READ | PROT_WRITE), ==, 0);
		memcpy (map + page - length, input, length);
		g_assert_cmpint (mprotect (map, page, PROT_READ), ==, 0);
		_g_mime_encoding_validator_write (validator, map + page - length, length);
		_g_mime_encoding_validator_flush (validator);
		g_assert_cmpuint (log.length, ==, length < sizeof input - 3 ? 1 : 0);
		if (log.length) {
			g_assert_cmpint (log.issues[0].violation, ==, GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT);
			g_assert_cmpint (log.issues[0].stream_offset, ==, length);
		}
		g_object_unref (validator);
	}
	g_assert_cmpint (munmap (map, page * 2), ==, 0);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

static void
check_scans (const guint8 *input, gsize length)
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	GMimeCodecScan actual, expected = { 0 };
	gsize i, qp;

	for (i = 0; i < length; i++) {
		if (memchr (alphabet, input[i], sizeof alphabet - 1)) {
			expected.alphabet_count++;
		} else if (input[i] == '\n') {
			expected.line_feed_count++;
			expected.line_begin_offset = i + 1;
		} else if (input[i] != '\r' && input[i] != ' ' && input[i] != '\t') {
			break;
		}
	}
	g_assert_cmpuint (_g_mime_codec_scan_base64 (input, length, &actual), ==, i);
	g_assert_cmpuint (actual.alphabet_count, ==, expected.alphabet_count);
	g_assert_cmpuint (actual.line_feed_count, ==, expected.line_feed_count);
	g_assert_cmpuint (actual.line_begin_offset, ==, expected.line_begin_offset);

	for (qp = 0; qp < length; qp++) {
		if (input[qp] == '=' || input[qp] == '\n')
			break;
	}
	g_assert_cmpuint (_g_mime_codec_index_of_qp_special (input, length), ==, qp);
}

static void
test_scan_kernels (void)
{
	guint8 storage[96], *input;
	guint value, lane, alignment, i;
	gsize length;

	check_scans (NULL, 0);
	for (value = 0; value < 256; value++) {
		input = storage + value % 16;
		for (lane = 0; lane < 32; lane++) {
			memset (input, 'A', 80);
			input[lane] = (guint8) value;
			input[lane + 1] = '\n';
			input[79] = '?';
			check_scans (input, 80);
		}

		for (i = 0; i < 80; i++)
			input[i] = value & (1u << (i % 8)) ? '\n' : 'A';
		check_scans (input, 80);
	}
	for (alignment = 0; alignment < 16; alignment++) {
		input = storage + alignment;
		for (i = 0; i < 80; i++)
			input[i] = "Ab9+/\r\n \t"[i % 9];
		for (length = 0; length <= 80; length++)
			check_scans (input, length);
	}
}

static void
test_vector_diagnostics (void)
{
	static const GMimeEncodingIssue base64_issues[] = {
		{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 0, 1, 1 },
		{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 1, 1, 2 },
		{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 32, 3, 1 },
		{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 33, 3, 2 },
		{ GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER, 64, 5, 1 },
		{ GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT, 65, 5, 2 }
	};
	static const GMimeEncodingIssue qp_issues[] = {
		{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 17, 1, 18 },
		{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 33, 2, 2 },
		{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK, 65, 3, 17 },
		{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 67, 3, 19 },
		{ GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING, 81, 4, 2 }
	};
	guint8 input[96];

	memset (input, 'A', sizeof input);
	input[0] = input[32] = input[64] = '?';
	input[1] = input[33] = input[65] = '*';
	input[16] = input[31] = input[48] = input[63] = input[80] = input[95] = '\n';
	check_input (0, input, sizeof input, base64_issues, G_N_ELEMENTS (base64_issues));

	memset (input, 'A', sizeof input);
	input[16] = input[32] = input[63] = input[66] = input[80] = '=';
	input[17] = input[33] = input[67] = input[81] = '~';
	input[31] = input[48] = input[79] = input[95] = '\n';
	input[64] = '\r';
	input[65] = '?';
	check_input (1, input, sizeof input, qp_issues, G_N_ELEMENTS (qp_issues));
}

static void
test_guard_pages (void)
{
#ifdef G_OS_UNIX
	gsize page = (gsize) sysconf (_SC_PAGESIZE);
	guint8 *map = mmap (NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	guint kind, length;

	g_assert_true (map != MAP_FAILED);
	memset (map, 'A', page);
	g_assert_cmpint (mprotect (map, page, PROT_READ), ==, 0);
	g_assert_cmpint (mprotect (map + page, page, PROT_NONE), ==, 0);
	for (kind = 0; kind < 2; kind++) {
		for (length = 0; length <= 65; length++) {
			IssueLog log = { 0 };
			GMimeEncodingValidator *validator = new_validator (kind, &log, 0, 1);

			_g_mime_encoding_validator_write (validator, map + page - length, length);
			_g_mime_encoding_validator_flush (validator);
			g_assert_cmpuint (log.length, ==, kind == 0 && length % 4 != 0 ? 1 : 0);
			if (log.length) {
				g_assert_cmpint (log.issues[0].violation, ==, GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM);
				g_assert_cmpint (log.issues[0].stream_offset, ==, length);
				g_assert_cmpuint (log.issues[0].column_number, ==, length + 1);
			}
			g_object_unref (validator);
		}
	}
	for (length = 0; length <= 80; length++)
		check_scans (map + page - length, length);
	g_assert_cmpint (mprotect (map, page, PROT_READ | PROT_WRITE), ==, 0);
	for (length = 0; length < 80; length++)
		map[page - 80 + length] = (guint8) length;
	g_assert_cmpint (mprotect (map, page, PROT_READ), ==, 0);
	for (length = 0; length <= 80; length++)
		check_scans (map + page - length, length);
	g_assert_cmpint (munmap (map, page * 2), ==, 0);
#else
	g_test_skip ("Guard pages require mmap");
#endif
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/encoding-validators/base64", test_base64);
	g_test_add_func ("/encoding-validators/quoted-printable", test_quoted_printable);
	g_test_add_func ("/encoding-validators/uuencode", test_uuencode);
	g_test_add_func ("/encoding-validators/uu-file-mode", test_uu_file_mode);
	g_test_add_func ("/encoding-validators/uu-flush", test_uu_flush);
	g_test_add_func ("/encoding-validators/flush", test_flush);
	g_test_add_func ("/encoding-validators/preconditions", test_preconditions);
	g_test_add_func ("/encoding-validators/scan-kernels", test_scan_kernels);
	g_test_add_func ("/encoding-validators/vector-diagnostics", test_vector_diagnostics);
	g_test_add_func ("/encoding-validators/guard-pages", test_guard_pages);
	g_test_add_func ("/encoding-validators/uu-guard-pages", test_uu_guard_pages);
	return g_test_run ();
}
