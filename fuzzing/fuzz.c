/* Adapted from MimeKit.Fuzzing.Helpers.Fuzz and MimeComplianceChecker.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <stdlib.h>
#include <string.h>

const FuzzTarget fuzz_targets[] = {
	{ "mime-reader", fuzz_mime_reader, FUZZ_MAX_LENGTH },
	{ "mime-parser", fuzz_mime_parser, FUZZ_MAX_LENGTH },
	{ "encoders", fuzz_encoders, 256 * 1024 },
	{ "decoders", fuzz_decoders, FUZZ_MAX_LENGTH },
	{ "rfc2047", fuzz_rfc2047, 64 * 1024 },
};
const gsize fuzz_target_count = G_N_ELEMENTS (fuzz_targets);

typedef struct {
	GMimeStreamMem parent_instance;
	gsize chunk;
} FuzzInput;

typedef GMimeStreamMemClass FuzzInputClass;

G_DEFINE_TYPE (FuzzInput, fuzz_input, GMIME_TYPE_STREAM_MEM)

static ssize_t
fuzz_input_read (GMimeStream *stream, char *buffer, size_t length)
{
	FuzzInput *input = (FuzzInput *) stream;

	return GMIME_STREAM_CLASS (fuzz_input_parent_class)->read (stream, buffer, MIN (length, input->chunk));
}

static void
fuzz_input_class_init (FuzzInputClass *klass)
{
	GMIME_STREAM_CLASS (klass)->read = fuzz_input_read;
}

static void
fuzz_input_init (FuzzInput *input)
{
	input->chunk = 4096;
}

void
fuzz_init (void)
{
	g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
	g_mime_init ();
}

guint64
fuzz_hash (const guint8 *data, gsize length, guint64 hash)
{
	gsize i;

	for (i = 0; i < length; i++) {
		hash ^= data[i];
		hash *= G_GUINT64_CONSTANT (1099511628211);
	}
	return hash;
}

guint32
fuzz_selector (const guint8 *data, gsize length)
{
	guint64 hash = fuzz_hash (data, length, FUZZ_HASH_INIT);

	/* Keep MIME prefixes intact while varying the configuration. */
	return (guint32) (hash ^ (hash >> 32));
}

void
fuzz_equal (const guint8 *expected, gsize expected_length, const guint8 *actual, gsize actual_length, const char *what)
{
	gsize i;

	for (i = 0; i < MIN (expected_length, actual_length); i++)
		if (expected[i] != actual[i])
			break;
	if (i != expected_length || i != actual_length) {
		char *first = g_base64_encode (i < expected_length ? expected + i : (const guint8 *) "",
					     MIN (64, expected_length - i));
		char *second = g_base64_encode (i < actual_length ? actual + i : (const guint8 *) "",
					      MIN (64, actual_length - i));

		g_error ("%s: buffers differ at %" G_GSIZE_FORMAT " (lengths %" G_GSIZE_FORMAT " vs %" G_GSIZE_FORMAT
			 "; next bytes in base64: %s vs %s)", what, i, expected_length, actual_length, first, second);
	}
}

GMimeParserOptions *
fuzz_parser_options (void)
{
	GMimeParserOptions *options = g_mime_parser_options_new ();

	g_mime_parser_options_set_max_mime_depth (options, 16);
	g_mime_parser_options_set_max_address_group_depth (options, 4);
	return options;
}

GMimeStream *
fuzz_stream_new (const guint8 *data, gsize length, gsize chunk)
{
	FuzzInput *input = g_object_new (fuzz_input_get_type (), NULL);
	GByteArray *bytes = g_byte_array_new ();

	input->chunk = chunk;
	g_byte_array_append (bytes, data, length);
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) input, bytes);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) input, TRUE);
	return (GMimeStream *) input;
}

void
fuzz_check_error (GError *error)
{
	FUZZ_CHECK (error && error->domain == GMIME_ERROR &&
		    (error->code == GMIME_ERROR_PARSE_ERROR || error->code == GMIME_ERROR_HEADER_TOO_LONG),
		    "Unexpected error: %s", error ? error->message : "(none)");
}

void
fuzz_compliance_init (FuzzCompliance *checker, gsize length, guint32 selector)
{
	static const guint caps[] = { 1, 4, 0 };

	memset (checker, 0, sizeof (*checker));
	checker->trace = g_string_new (NULL);
	checker->length = length;
	checker->cap = (selector & 3) < G_N_ELEMENTS (caps) ? caps[selector & 3] :
		g_mime_compliance_options_get_max_issues_per_violation (g_mime_compliance_options_get_default ());
	checker->context = (selector & 4) ? GMIME_COMPLIANCE_CONTEXT_STORAGE : GMIME_COMPLIANCE_CONTEXT_TRANSPORT;
}

void
fuzz_compliance_begin (FuzzCompliance *checker)
{
	memset (checker->counts, 0, sizeof (checker->counts));
	checker->reached_cap = checker->truncated = FALSE;
	g_string_append (checker->trace, "--\n");
	checker->operation_start = checker->trace->len;
}

static int
compare_issues (const void *first, const void *second)
{
	return strcmp (*(char * const *) first, *(char * const *) second);
}

void
fuzz_compliance_end (FuzzCompliance *checker)
{
	char **issues = g_strsplit (checker->trace->str + checker->operation_start, "\n", -1);
	guint i;

	/* Independent validators can emit the same issues in different orders
	 * when a stream fragment ends inside a header or encoded body. */
	qsort (issues, g_strv_length (issues), sizeof (char *), compare_issues);
	g_string_truncate (checker->trace, checker->operation_start);
	for (i = 0; issues[i]; i++)
		if (*issues[i])
			g_string_append_printf (checker->trace, "%s\n", issues[i]);
	g_strfreev (issues);
}

GMimeComplianceOptions *
fuzz_compliance_options (FuzzCompliance *checker)
{
	GMimeComplianceOptions *options = g_mime_compliance_options_new ();

	g_mime_compliance_options_set_context (options, checker->context);
	g_mime_compliance_options_set_enabled_validators (options, GMIME_COMPLIANCE_VALIDATOR_ALL);
	g_mime_compliance_options_set_max_issues_per_violation (options, checker->cap);

	return options;
}

void
fuzz_compliance_attach (FuzzCompliance *checker, GMimeReader *reader)
{
	GMimeComplianceOptions *options = fuzz_compliance_options (checker);

	g_mime_reader_set_compliance_options (reader, options);
	g_mime_compliance_options_free (options);
	g_mime_reader_set_compliance_logger (reader, fuzz_compliance_report, checker, NULL);
}

void
fuzz_compliance_report (const GMimeComplianceIssue *issue, gpointer user_data)
{
	FuzzCompliance *checker = user_data;
	GMimeComplianceViolation violation = issue->violation;
	const char *description;

	FUZZ_CHECK ((violation > GMIME_COMPLIANCE_NONE && violation <= GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT) ||
		    violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES, "Undefined compliance violation: %d", violation);
	FUZZ_CHECK (issue->position_kind >= GMIME_COMPLIANCE_POSITION_EXACT &&
		    issue->position_kind <= GMIME_COMPLIANCE_POSITION_ELEMENT_START, "Undefined compliance position kind");
	FUZZ_CHECK (issue->context == checker->context, "Compliance context changed");
	FUZZ_CHECK (issue->stream_offset >= 0 && (guint64) issue->stream_offset <= checker->length,
		    "Compliance offset outside input: %" G_GINT64_FORMAT, issue->stream_offset);
	FUZZ_CHECK (issue->line_number >= 1, "Invalid compliance line number");
	description = g_mime_compliance_violation_get_description (violation);
	FUZZ_CHECK (description && *description, "Missing compliance description");
	FUZZ_CHECK (g_mime_compliance_violation_get_severity (violation, issue->context) >= GMIME_COMPLIANCE_SEVERITY_MINOR &&
		    g_mime_compliance_violation_get_severity (violation, issue->context) <= GMIME_COMPLIANCE_SEVERITY_CRITICAL,
		    "Invalid compliance severity");
	FUZZ_CHECK ((g_mime_compliance_violation_get_categories (violation) & ~7) == 0, "Invalid compliance categories");
	if (violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES) {
		FUZZ_CHECK (checker->cap && checker->reached_cap && !checker->truncated, "Invalid compliance truncation report");
		checker->truncated = TRUE;
	} else {
		guint count = ++checker->counts[violation];

		if (checker->cap) {
			FUZZ_CHECK (count <= checker->cap, "Compliance violation %d exceeded its budget", violation);
			if (count == checker->cap)
				checker->reached_cap = TRUE;
		}
	}
	g_string_append_printf (checker->trace, "%d:%d:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT "\n",
				violation, issue->position_kind, issue->stream_offset, issue->line_number, issue->column_number);
}
