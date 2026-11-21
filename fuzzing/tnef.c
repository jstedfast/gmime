/* Adapted from MimeKit.Fuzzing.Targets.TnefReaderTarget and TnefMessageTarget.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

static GMimeTnefOptions *
tnef_options (void)
{
	GMimeTnefOptions *options = g_mime_tnef_options_new ();

	g_mime_tnef_options_set_max_nesting_depth (options, 4);
	g_mime_tnef_options_set_max_property_value_length (options, FUZZ_MAX_LENGTH);
	g_mime_tnef_options_set_max_total_data_bytes (options, 16 * FUZZ_MAX_LENGTH);
	return options;
}

typedef struct {
	gsize length;
	guint cap;
	guint counts[GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED + 1];
	gboolean reported_limit;
	GString *trace;
} Diagnostics;

static void
record_issue (const GMimeTnefComplianceIssue *issue, gpointer user_data)
{
	Diagnostics *checker = user_data;

	FUZZ_CHECK (issue->stream_offset >= 0 && (guint64) issue->stream_offset <= checker->length,
		    "TNEF diagnostic outside the input");
	if (issue->violation == GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES) {
		FUZZ_CHECK (!checker->reported_limit, "Duplicate TNEF issue-limit notification");
		checker->reported_limit = TRUE;
	} else {
		FUZZ_CHECK (issue->violation > 0 && (guint) issue->violation < G_N_ELEMENTS (checker->counts),
			    "Unknown TNEF diagnostic");
		checker->counts[issue->violation]++;
		FUZZ_CHECK (!checker->cap || checker->counts[issue->violation] <= checker->cap,
			    "TNEF issue budget exceeded");
	}
	g_string_append_printf (checker->trace, "D%d:%" G_GINT64_FORMAT ":%u:%u:%u;", issue->violation,
				issue->stream_offset, issue->depth, issue->attribute_tag, issue->property_tag);
}

static GString *
walk (const guint8 *data, gsize length, gsize chunk, guint cap)
{
	GMimeStream *input = fuzz_stream_new (data, length, chunk);
	GMimeTnefOptions *options = tnef_options ();
	GMimeTnefReader *reader = g_mime_tnef_reader_new (input, options);
	GMimeTnefComplianceOptions *limits = g_mime_tnef_compliance_options_new ();
	Diagnostics checker = { length, cap, { 0 }, FALSE, g_string_new (NULL) };
	GError *error = NULL;
	gint64 previous = -1;

	g_mime_tnef_compliance_options_set_max_issues_per_violation (limits, cap);
	g_mime_tnef_reader_set_compliance_options (reader, limits);
	g_mime_tnef_reader_set_compliance_logger (reader, record_issue, &checker, NULL);
	while (g_mime_tnef_reader_read (reader, &error)) {
		gint64 offset = g_mime_tnef_reader_get_stream_offset (reader);
		GMimeTnefAttributeTag tag = g_mime_tnef_reader_get_tag (reader);
		GBytes *value;

		FUZZ_CHECK (offset > previous && (guint64) offset < length, "TNEF reader failed to advance");
		previous = offset;
		value = g_mime_tnef_reader_read_value_as_bytes (reader, &error);
		if (!value)
			break;
		gsize size;
		const guint8 *bytes = g_bytes_get_data (value, &size);
		g_string_append_printf (checker.trace, "A%" G_GINT64_FORMAT ":%u:%u:%u:%" G_GSIZE_FORMAT
					":%" G_GUINT64_FORMAT ";", offset, g_mime_tnef_reader_get_level (reader),
					tag, g_mime_tnef_reader_get_length (reader), size, fuzz_hash (bytes, size, FUZZ_HASH_INIT));
		g_bytes_unref (value);
	}
	g_assert_no_error (error);
	g_mime_tnef_compliance_options_unref (limits);
	g_mime_tnef_options_unref (options);
	g_object_unref (reader);
	g_object_unref (input);
	return checker.trace;
}

/* Write accepted raw attributes through the public writer. It supplies the
 * version and OEM codepage itself, and rejects invalid attribute ordering. */
static GByteArray *
reemit (const guint8 *data, gsize length)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeStream *input = fuzz_stream_new (data, length, 4096);
	GMimeTnefOptions *options = tnef_options ();
	GMimeTnefReader *reader = g_mime_tnef_reader_new (input, options);
	GMimeTnefWriter *writer = g_mime_tnef_writer_new (output, 1252, 0, NULL);
	GError *error = NULL;
	GByteArray *result = NULL;

	while (g_mime_tnef_reader_read (reader, &error)) {
		GMimeTnefAttributeTag tag = g_mime_tnef_reader_get_tag (reader);
		GBytes *value;
		gsize size;
		const guint8 *bytes;
		gboolean accepted;

		if (tag == GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION || tag == GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE)
			continue;
		value = g_mime_tnef_reader_read_value_as_bytes (reader, &error);
		if (!value)
			break;
		bytes = g_bytes_get_data (value, &size);
		accepted = g_mime_tnef_writer_write_attribute_bytes (writer, tag, bytes, size, &error);
		g_bytes_unref (value);
		if (!accepted) {
			g_assert_nonnull (error);
			break;
		}
	}
	if (!error && g_mime_tnef_writer_close (writer, &error))
		result = g_byte_array_ref (g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output)));
	g_clear_error (&error);
	g_object_unref (writer);
	g_object_unref (reader);
	g_mime_tnef_options_unref (options);
	g_object_unref (input);
	g_object_unref (output);
	return result;
}

void
fuzz_tnef_reader (const guint8 *data, gsize length)
{
	static const guint caps[] = { 0, 1, 4 };
	guint selector = fuzz_selector (data, length);
	guint cap = caps[selector % G_N_ELEMENTS (caps)];
	GString *whole = walk (data, length, MAX (length, 1), cap);
	GString *split = walk (data, length, 1 + selector % 127, cap);

	fuzz_equal ((const guint8 *) whole->str, whole->len, (const guint8 *) split->str, split->len, "TNEF reader trace");
	g_string_free (whole, TRUE);
	g_string_free (split, TRUE);

	if (length >= 6 && !memcmp (data, "\x78\x9f\x3e\x22", 4)) {
		GByteArray *first = reemit (data, length);

		if (first) {
			GByteArray *second = reemit (first->data, first->len);
			FUZZ_CHECK (second, "TNEF writer rejected its own output");
			fuzz_equal (first->data, first->len, second->data, second->len, "TNEF re-emission");
			g_byte_array_unref (second);
			g_byte_array_unref (first);
		}
	}
}

static void
compare_body (GMimeTnefMessageBody *first, GMimeTnefMessageBody *second)
{
	g_assert_true ((first == NULL) == (second == NULL));
	if (first) {
		g_assert_cmpuint (g_mime_tnef_message_body_get_tag (first), ==, g_mime_tnef_message_body_get_tag (second));
		g_assert_cmpint (g_mime_tnef_message_body_get_length (first), ==, g_mime_tnef_message_body_get_length (second));
		g_assert_cmpstr (g_mime_tnef_message_body_get_charset (first), ==, g_mime_tnef_message_body_get_charset (second));
	}
}

void
fuzz_tnef_message (const guint8 *data, gsize length)
{
	guint selector = fuzz_selector (data, length);
	GMimeTnefOptions *options = tnef_options ();
	GMimeStream *input = fuzz_stream_new (data, length, MAX (length, 1));
	GMimeStream *split = fuzz_stream_new (data, length, 1 + selector % 127);
	GMimeTnefReader *reader = g_mime_tnef_reader_new (split, options);
	GError *error = NULL, *other_error = NULL;
	GMimeTnefMessage *first = g_mime_tnef_message_load (input, options, &error);
	GMimeTnefMessage *second = g_mime_tnef_message_load_from_reader (reader, &other_error);
	GMimeTnefConversionOptions *conversion;
	GMimeTnefConversionResult *result;

	g_object_unref (input);
	g_object_unref (reader);
	g_object_unref (split);
	g_mime_tnef_options_unref (options);
	g_assert_true ((first == NULL) == (second == NULL));
	if (!first) {
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_assert_error (other_error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		FUZZ_CHECK (length < 4 || memcmp (data, "\x78\x9f\x3e\x22", 4), "Rejected a valid TNEF signature");
		g_error_free (error);
		g_error_free (other_error);
		return;
	}
	g_assert_no_error (error);
	g_assert_no_error (other_error);
	g_assert_cmpstr (g_mime_tnef_message_get_subject (first), ==, g_mime_tnef_message_get_subject (second));
	g_assert_cmpstr (g_mime_tnef_message_get_message_class (first), ==, g_mime_tnef_message_get_message_class (second));
	g_assert_cmpuint (g_mime_tnef_message_get_attachment_count (first), ==, g_mime_tnef_message_get_attachment_count (second));
	g_assert_cmpuint (g_mime_tnef_message_get_recipient_count (first), ==, g_mime_tnef_message_get_recipient_count (second));
	compare_body (g_mime_tnef_message_get_text_body (first), g_mime_tnef_message_get_text_body (second));
	compare_body (g_mime_tnef_message_get_html_body (first), g_mime_tnef_message_get_html_body (second));
	compare_body (g_mime_tnef_message_get_rtf_body (first), g_mime_tnef_message_get_rtf_body (second));
	g_object_unref (second);

	conversion = g_mime_tnef_conversion_options_new ();
	g_mime_tnef_conversion_options_set_convert_embedded_messages (conversion, (selector & 1) != 0);
	g_mime_tnef_conversion_options_set_generate_calendar (conversion, (selector & 2) != 0);
	g_mime_tnef_conversion_options_set_max_calendar_exceptions (conversion, 64);
	g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (conversion, FUZZ_MAX_LENGTH);
	result = g_mime_tnef_message_convert_to_mime (first, conversion, &error);
	g_assert_no_error (error);
	g_assert_nonnull (result);
	g_object_unref (first);
	fuzz_check_mime_roundtrip (g_mime_tnef_conversion_result_get_message (result));
	g_mime_tnef_conversion_result_unref (result);
	g_mime_tnef_conversion_options_unref (conversion);
}
