/* Adapted from MimeKit.Fuzzing.Targets.MimeParserTarget and Helpers.MimeRoundTrip.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

static GByteArray *
serialize (GMimeMessage *message)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *bytes;
	ssize_t written;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	written = g_mime_entity_write_to_stream ((GMimeEntity *) message, options, stream);
	bytes = g_byte_array_ref (g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream));
	FUZZ_CHECK (written >= 0 && (guint64) written == bytes->len, "Serialization failed or returned an incorrect byte count");
	g_object_unref (stream);
	g_mime_format_options_free (options);
	return bytes;
}

static gboolean
equal_ignoring_cr (GByteArray *first, GByteArray *second)
{
	guint i = 0, j = 0;

	for (;;) {
		while (i < first->len && first->data[i] == '\r')
			i++;
		while (j < second->len && second->data[j] == '\r')
			j++;
		if (i == first->len || j == second->len)
			return i == first->len && j == second->len;
		if (first->data[i++] != second->data[j++])
			return FALSE;
	}
}

static void
check_roundtrip (GMimeMessage *message)
{
	GMimeParserOptions *options = fuzz_parser_options ();
	GByteArray *first = serialize (message), *second;
	guint round = 0;

	/* Generated headers may exceed the original input's header limit. */
	FUZZ_CHECK (first->len <= G_MAXINT, "Serialized headers exceed the parser's representable limit");
	g_mime_parser_options_set_max_header_length (options,
		MAX (g_mime_parser_options_get_max_header_length (options), (int) first->len));
	for (;;) {
		GMimeStream *stream = fuzz_stream_new (first->data, first->len, 4096);
		GMimeParser *parser = g_mime_parser_new_with_stream (stream);
		GError *error = NULL;
		GMimeMessage *reparsed = g_mime_parser_construct_message (parser, options, &error);

		FUZZ_CHECK (reparsed && error == NULL, "Parser rejected its own output: %s", error ? error->message : "(EOF)");
		second = serialize (reparsed);
		g_object_unref (reparsed);
		g_object_unref (parser);
		g_object_unref (stream);
		if (first->len == second->len && (!first->len || !memcmp (first->data, second->data, first->len)))
			break;
		/* Like MimeKit, allow only CR removal to converge on malformed CR runs. */
		if (!equal_ignoring_cr (first, second))
			fuzz_equal (first->data, first->len, second->data, second->len, "Parser serialization fixed point");
		if (++round == 16) {
			FUZZ_CHECK (second->len < first->len, "CR-only round trips did not converge or shrink");
			break;
		}
		g_byte_array_unref (first);
		first = second;
	}
	g_byte_array_unref (first);
	g_byte_array_unref (second);
	g_mime_parser_options_free (options);
}

static GString *
parse_input (const guint8 *data, gsize length, GMimeFormat format, gboolean source, gsize chunk,
	     gboolean logged, gboolean roundtrip)
{
	GMimeParserOptions *options = fuzz_parser_options ();
	GMimeStream *stream = fuzz_stream_new (data, length, chunk);
	GMimeParser *parser;
	GMimeMessage *last_message = NULL;
	GByteArray *last_bytes = NULL;
	GString *result = g_string_new (NULL);
	FuzzCompliance checker;
	GError *error = NULL;
	gint64 before = 0;
	guint count = 0;

	fuzz_compliance_init (&checker, length, fuzz_selector (data, length));
	parser = g_mime_parser_new_with_stream_and_storage (stream, format,
		source ? GMIME_CONTENT_STORAGE_SOURCE : GMIME_CONTENT_STORAGE_MEMORY, &error);
	FUZZ_CHECK (parser && error == NULL, "Failed to construct parser for a memory stream");
	if (logged) {
		GMimeComplianceOptions *compliance = fuzz_compliance_options (&checker);

		g_mime_parser_set_compliance_options (parser, compliance);
		g_mime_compliance_options_free (compliance);
		g_mime_parser_set_compliance_logger (parser, fuzz_compliance_report, &checker, NULL);
	}
	do {
		GMimeMessage *message;
		GByteArray *bytes;
		gint64 after;
		gboolean eos;

		fuzz_compliance_begin (&checker);
		message = g_mime_parser_construct_message (parser, options, &error);
		after = g_mime_parser_tell (parser);
		eos = g_mime_parser_eos (parser);
		FUZZ_CHECK (after >= before && (guint64) after <= length, "Parser position outside input");
		g_string_append_printf (result, "RESULT:%d:%" G_GINT64_FORMAT ":%d\n",
					message != NULL, error ? -1 : after, !error && eos);
		if (error) {
			fuzz_check_error (error);
			FUZZ_CHECK (message == NULL, "Parser returned a model with an error");
			g_string_append_printf (result, "ERROR:%s\n", error->message);
			g_clear_error (&error);
			break;
		}
		if (!message) {
			FUZZ_CHECK (eos, "Parser returned neither a model nor an error before EOF");
			break;
		}
		FUZZ_CHECK (after > before || eos, "Parser did not advance");
		before = after;
		if (format == GMIME_FORMAT_MBOX) {
			gint64 offset = g_mime_parser_get_mbox_marker_offset (parser);
			char *marker = g_mime_parser_get_mbox_marker (parser);
			FUZZ_CHECK (marker && offset >= 0 && (guint64) offset <= length, "Invalid mailbox marker");
			g_string_append_printf (result, "M:%" G_GINT64_FORMAT ":%s\n", offset, marker);
			g_free (marker);
		}
		bytes = serialize (message);
		g_string_append_printf (result, "BYTES:%u\n", bytes->len);
		g_string_append_len (result, (const char *) bytes->data, bytes->len);
		g_clear_object (&last_message);
		g_clear_pointer (&last_bytes, g_byte_array_unref);
		last_message = message;
		last_bytes = bytes;
		if (format == GMIME_FORMAT_MESSAGE || eos)
			break;
	} while (++count < FUZZ_MAX_MESSAGES);
	if (count == FUZZ_MAX_MESSAGES)
		g_string_append (result, "BUDGET\n");
	g_clear_object (&parser);
	g_object_unref (stream);
	if (last_message) {
		GByteArray *again = serialize (last_message);

		fuzz_equal (last_bytes->data, last_bytes->len, again->data, again->len, "Repeated write or source-content lifetime");
		g_byte_array_unref (again);
		if (roundtrip && format == GMIME_FORMAT_MESSAGE)
			check_roundtrip (last_message);
		g_object_unref (last_message);
		g_byte_array_unref (last_bytes);
	}
	g_string_free (checker.trace, TRUE);
	g_mime_parser_options_free (options);
	return result;
}

void
fuzz_mime_parser (const guint8 *data, gsize length)
{
	static const gsize chunks[] = { 1, 7, 31, 4096 };
	guint32 selector = fuzz_selector (data, length);
	GMimeFormat format;

	for (format = GMIME_FORMAT_MESSAGE; format <= GMIME_FORMAT_MBOX; format++) {
		GString *memory, *source, *logged;
		gboolean persist = (selector & 1) != 0;

		if (format == GMIME_FORMAT_MBOX && (length < 5 || memcmp (data, "From ", 5)))
			continue;
		memory = parse_input (data, length, format, FALSE, 4096, FALSE, TRUE);
		source = parse_input (data, length, format, TRUE, chunks[(selector >> 3) & 3], FALSE, FALSE);
		logged = parse_input (data, length, format, persist, 4096, TRUE, FALSE);
		fuzz_equal ((guint8 *) memory->str, memory->len, (guint8 *) source->str, source->len, "Parser memory vs source/chunked");
		fuzz_equal ((guint8 *) memory->str, memory->len, (guint8 *) logged->str, logged->len, "Parser logging changed output");
		g_string_free (memory, TRUE);
		g_string_free (source, TRUE);
		g_string_free (logged, TRUE);
	}
}
