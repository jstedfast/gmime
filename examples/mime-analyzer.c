/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Port of MimeKit's MimeAnalyzer sample by Jeffrey Stedfast.
 *
 * Copyright (c) 2026 Jeffrey Stedfast
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <gmime/gmime.h>

typedef struct {
	char **files;
	char *context_name;
	char *severity_name;
	char *category_names;
	GMimeComplianceContext context;
	GMimeComplianceSeverity minimum_severity;
	GMimeComplianceCategories categories;
	gboolean werror, no_caret, remarks, no_color, quiet;
	int max_issues, max_per_violation;
} Options;

typedef struct {
	GArray *issues;
	guint limit;
	gboolean truncated;
} Collector;

typedef struct {
	char *bytes;
	gsize length;
	GArray *lines;
} SourceText;

typedef struct {
	gboolean color, show_source, show_remarks;
	int gutter_width;
	gsize max_width;
} DiagnosticWriter;

typedef struct {
	guint64 errors, warnings, notes;
} Counts;

static void
collect_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
	Collector *collector = user_data;

	/* Suppression is not a message defect and must not affect --werror. */
	if (issue->violation == GMIME_COMPLIANCE_TOO_MANY_COMPLIANCE_ISSUES)
		collector->truncated = TRUE;
	else if (collector->issues->len < collector->limit)
		g_array_append_val (collector->issues, *issue);
	else
		collector->truncated = TRUE;
}

static void
source_index_lines (SourceText *source)
{
	gsize i = 0;

	source->lines = g_array_new (FALSE, FALSE, sizeof (gsize));
	g_array_append_val (source->lines, i);

	for ( ; i < source->length; i++) {
		if (source->bytes[i] == '\n' && i + 1 < source->length) {
			gsize next = i + 1;
			g_array_append_val (source->lines, next);
		}
	}
}

static gboolean
source_get_line (const SourceText *source, guint64 line, gsize *start, gsize *end)
{
	if (line < 1 || line > source->lines->len)
		return FALSE;

	*start = g_array_index (source->lines, gsize, line - 1);
	*end = line < source->lines->len ? g_array_index (source->lines, gsize, line) : source->length;

	if (*end > *start && source->bytes[*end - 1] == '\n')
		(*end)--;

	if (*end > *start && source->bytes[*end - 1] == '\r')
		(*end)--;

	return TRUE;
}

static void
source_get_position (const SourceText *source, gsize offset, guint64 *line, guint64 *column)
{
	guint lo = 0, hi = source->lines->len - 1;

	while (lo < hi) {
		guint mid = lo + (hi - lo + 1) / 2;

		if (g_array_index (source->lines, gsize, mid) <= offset)
			lo = mid;
		else
			hi = mid - 1;
	}

	*line = (guint64) lo + 1;
	*column = offset - g_array_index (source->lines, gsize, lo) + 1;
}

static void
source_locate_issue (const SourceText *source, const GMimeComplianceIssue *issue,
		     guint64 *line, guint64 *column)
{
	gboolean non_ascii;
	gsize start, end, i;

	*line = issue->line_number;
	*column = issue->column_number;

	if (issue->position_kind == GMIME_COMPLIANCE_POSITION_EXACT)
		return;

	if (issue->violation == GMIME_COMPLIANCE_BLANK_HEADER_CONTINUATION_LINE) {
		if (source_get_line (source, *line, &start, &end))
			*column = end - start + 1;
		return;
	}

	switch (issue->violation) {
	case GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_HEADER:
	case GMIME_COMPLIANCE_UNEXPECTED_8BIT_BYTES_IN_BODY:
		non_ascii = TRUE;
		break;
	case GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_HEADER:
	case GMIME_COMPLIANCE_UNEXPECTED_NULL_BYTES_IN_BODY:
		non_ascii = FALSE;
		break;
	default:
		return;
	}

	if (issue->stream_offset < 0 || (guint64) issue->stream_offset >= source->length)
		return;

	start = issue->stream_offset;
	end = source->length;

	if (issue->position_kind == GMIME_COMPLIANCE_POSITION_LINE_START &&
	    *line > 0 && *line < source->lines->len)
		end = g_array_index (source->lines, gsize, *line);

	for (i = start; i < end; i++) {
		guint8 c = (guint8) source->bytes[i];

		if (non_ascii ? c > 0x7f : c == 0) {
			source_get_position (source, i, line, column);
			return;
		}
	}
}

static char *
render_line (const SourceText *source, gsize start, gsize end, guint64 column, gsize *caret)
{
	GString *text = g_string_new (NULL);
	gsize i;

	*caret = 0;

	for (i = start; i < end; i++) {
		guint8 c = (guint8) source->bytes[i];

		if (i - start + 1 == column)
			*caret = text->len + 1;

		if (c == '\t') {
			guint spaces = 8 - text->len % 8;

			while (spaces--)
				g_string_append_c (text, ' ');
		} else if (c >= 0x20 && c < 0x7f) {
			g_string_append_c (text, c);
		} else {
			g_string_append_printf (text, "\\x%02X", c);
		}
	}

	if (column > 0 && column - 1 >= end - start)
		*caret = text->len + 1;

	return g_string_free (text, FALSE);
}

static char *
elide_line (char *text, gsize *caret, gsize max_width)
{
	gsize length = strlen (text), budget, start, end;
	GString *elided;

	if (length <= max_width)
		return text;

	budget = MAX ((gsize) 16, max_width - 6);
	start = *caret > budget / 2 + 1 ? *caret - 1 - budget / 2 : 0;
	end = MIN (length, start + budget);
	start = end > budget ? end - budget : 0;
	elided = g_string_new (start ? "..." : "");
	g_string_append_len (elided, text + start, end - start);

	if (end < length)
		g_string_append (elided, "...");

	if (*caret > 0)
		*caret = MIN (*caret - start + (start ? 3 : 0), elided->len + 1);

	g_free (text);

	return g_string_free (elided, FALSE);
}

static void
write_color (const DiagnosticWriter *writer, const char *color, const char *text)
{
	if (writer->color)
		g_print ("\033[%sm%s\033[0m", color, text);
	else
		g_print ("%s", text);
}

static void
write_gutter (const DiagnosticWriter *writer, const char *label)
{
	char *text = g_strdup_printf ("%*s | ", writer->gutter_width, label);

	write_color (writer, "90", text);
	g_free (text);
}

static void
write_remarks (const DiagnosticWriter *writer, const GMimeComplianceIssue *issue)
{
	GMimeComplianceCategories categories = g_mime_compliance_violation_get_categories (issue->violation);
	GString *text = g_string_new ("= categories: ");
	char **words;
	guint i;

	if (categories & GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY)
		g_string_append (text, "Interoperability, ");
	if (categories & GMIME_COMPLIANCE_CATEGORY_DATA_LOSS)
		g_string_append (text, "DataLoss, ");
	if (categories & GMIME_COMPLIANCE_CATEGORY_SECURITY)
		g_string_append (text, "Security, ");
	if (categories)
		g_string_truncate (text, text->len - 2);
	else
		g_string_append (text, "none");

	g_string_append_c (text, '\n');
	write_gutter (writer, "");
	write_color (writer, "90", text->str);
	g_string_truncate (text, 0);

	words = g_strsplit_set (g_mime_compliance_violation_get_remarks (issue->violation), " \t\r\n", -1);
	for (i = 0; words[i]; i++) {
		if (!*words[i])
			continue;

		if (text->len && text->len + 1 + strlen (words[i]) > 88) {
			write_gutter (writer, "");
			g_print ("= %s\n", text->str);
			g_string_truncate (text, 0);
		}

		if (text->len)
			g_string_append_c (text, ' ');

		g_string_append (text, words[i]);
	}

	if (text->len) {
		write_gutter (writer, "");
		g_print ("= %s\n", text->str);
	}

	g_strfreev (words);
	g_string_free (text, TRUE);
}

static void
write_issue (const DiagnosticWriter *writer, const char *filename, const SourceText *source,
	     const GMimeComplianceIssue *issue, gboolean werror)
{
	GMimeComplianceSeverity severity = g_mime_compliance_violation_get_severity (issue->violation, issue->context);
	const char *level, *color;
	guint64 line, column;
	gsize start, end, caret;
	char *text, *spaces;

	if (werror || severity == GMIME_COMPLIANCE_SEVERITY_CRITICAL) {
		level = "error";
		color = "1;31";
	} else if (severity == GMIME_COMPLIANCE_SEVERITY_MAJOR) {
		level = "warning";
		color = "1;33";
	} else {
		level = "note";
		color = "1;36";
	}

	source_locate_issue (source, issue, &line, &column);
	text = g_strdup_printf ("%s:%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT ": ", filename, line, column);
	write_color (writer, "1;37", text);
	g_free (text);

	text = g_strconcat (level, ": ", NULL);
	write_color (writer, color, text);
	g_free (text);

	write_color (writer, "1;37", g_mime_compliance_violation_get_description (issue->violation));
	text = g_strdup_printf (" [%s]\n", g_mime_compliance_violation_get_name (issue->violation));
	write_color (writer, "90", text);
	g_free (text);

	if (writer->show_source && source_get_line (source, line, &start, &end)) {
		text = render_line (source, start, end, column, &caret);
		text = elide_line (text, &caret, writer->max_width);
		spaces = g_strdup_printf ("%" G_GUINT64_FORMAT, line);

		write_gutter (writer, spaces);
		g_free (spaces);
		g_print ("%s\n", text);
		g_free (text);

		if (caret) {
			spaces = g_strnfill (caret - 1, ' ');
			text = g_strconcat (spaces, "^\n", NULL);
			write_gutter (writer, "");
			write_color (writer, color, text);
			g_free (spaces);
			g_free (text);
		}
	}

	if (writer->show_remarks)
		write_remarks (writer, issue);
}

static void
append_count (GString *text, guint64 count, const char *noun)
{
	if (text->len)
		g_string_append (text, ", ");

	g_string_append_printf (text, "%" G_GUINT64_FORMAT " %s%s", count, noun, count == 1 ? "" : "s");
}

static void
write_summary (const DiagnosticWriter *writer, const char *filename, const Counts *counts,
	       gboolean truncated, gboolean filtering)
{
	GString *text = g_string_new (NULL);
	char *prefix = g_strconcat (filename, ": ", NULL);

	write_color (writer, "1;37", prefix);
	g_free (prefix);

	if (counts->errors || counts->warnings || counts->notes) {
		if (counts->errors)
			append_count (text, counts->errors, "error");
		if (counts->warnings)
			append_count (text, counts->warnings, "warning");
		if (counts->notes)
			append_count (text, counts->notes, "note");

		g_string_append_c (text, '\n');
		write_color (writer, "1;37", text->str);
	} else {
		write_color (writer, "1;32", filtering ?
			     "no matching compliance issues\n" : "no compliance issues found\n");
	}

	if (truncated) {
		g_string_printf (text, "%s: note: issue limit reached, some issues were not reported\n", filename);
		write_color (writer, "90", text->str);
	}

	g_string_free (text, TRUE);
}

static gboolean
issue_matches (const GMimeComplianceIssue *issue, const Options *options)
{
	if (g_mime_compliance_violation_get_severity (issue->violation, issue->context) < options->minimum_severity)
		return FALSE;

	return !options->categories ||
		(g_mime_compliance_violation_get_categories (issue->violation) & options->categories) != 0;
}

static gboolean
analyze_file (const char *filename, const Options *options, DiagnosticWriter *writer, Counts *totals)
{
	Collector collector = { g_array_new (FALSE, FALSE, sizeof (GMimeComplianceIssue)), options->max_issues, FALSE };
	GMimeComplianceOptions *compliance;
	gboolean success, filtering;
	SourceText source = { 0 };
	Counts counts = { 0 };
	GError *error = NULL;
	GMimeStream *stream;
	GMimeReader *reader;
	guint i;

	if (!g_file_get_contents (filename, &source.bytes, &source.length, &error)) {
		g_printerr ("mime-analyzer: error: %s: %s\n", filename, error->message);
		g_error_free (error);
		g_array_unref (collector.issues);
		return FALSE;
	}

	if (source.length > G_MAXUINT) {
		g_printerr ("mime-analyzer: error: %s: message exceeds the memory stream size limit\n", filename);
		g_array_unref (collector.issues);
		g_free (source.bytes);
		return FALSE;
	}

	/* The stream owns the byte snapshot; retain it until diagnostics are written. */
	stream = g_mime_stream_mem_new_with_byte_array (
		g_byte_array_new_take ((guint8 *) source.bytes, source.length));
	source_index_lines (&source);

	reader = g_mime_reader_new_with_format (NULL, stream, GMIME_FORMAT_MESSAGE);
	compliance = g_mime_compliance_options_new ();

	g_mime_compliance_options_set_context (compliance, options->context);
	g_mime_compliance_options_set_max_issues_per_violation (compliance, options->max_per_violation);
	g_mime_reader_set_compliance_options (reader, compliance);
	g_mime_reader_set_compliance_logger (reader, collect_issue, &collector, NULL);

	success = g_mime_reader_read_message (reader, &error);
	g_mime_compliance_options_unref (compliance);
	g_object_unref (reader);

	if (!success) {
		g_printerr ("mime-analyzer: error: %s: %s\n", filename, error->message);
		g_error_free (error);
		goto done;
	}

	writer->gutter_width = 1;
	filtering = options->minimum_severity != GMIME_COMPLIANCE_SEVERITY_MINOR || options->categories;

	for (i = 0; i < collector.issues->len; i++) {
		const GMimeComplianceIssue *issue = &g_array_index (collector.issues, GMimeComplianceIssue, i);
		GMimeComplianceSeverity severity;
		guint64 line, column;
		char number[32];

		if (!issue_matches (issue, options))
			continue;

		severity = g_mime_compliance_violation_get_severity (issue->violation, issue->context);
		if (options->werror || severity == GMIME_COMPLIANCE_SEVERITY_CRITICAL)
			counts.errors++;
		else if (severity == GMIME_COMPLIANCE_SEVERITY_MAJOR)
			counts.warnings++;
		else
			counts.notes++;

		source_locate_issue (&source, issue, &line, &column);
		writer->gutter_width = MAX (writer->gutter_width,
					    g_snprintf (number, sizeof (number), "%" G_GUINT64_FORMAT, line));
	}

	if (!options->quiet) {
		for (i = 0; i < collector.issues->len; i++) {
			const GMimeComplianceIssue *issue = &g_array_index (collector.issues, GMimeComplianceIssue, i);

			if (issue_matches (issue, options))
				write_issue (writer, filename, &source, issue, options->werror);
		}

		if (counts.errors || counts.warnings || counts.notes)
			g_print ("\n");
	}

	write_summary (writer, filename, &counts, collector.truncated, filtering);
	totals->errors += counts.errors;
	totals->warnings += counts.warnings;
	totals->notes += counts.notes;

done:
	g_array_unref (collector.issues);
	g_array_unref (source.lines);
	g_object_unref (stream);
	return success;
}

static gboolean
parse_options (GOptionContext *context, int *argc, char ***argv, Options *options, GError **error)
{
	char **categories;
	guint i;

	if (!g_option_context_parse (context, argc, argv, error))
		return FALSE;

	if (!options->files) {
		g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "No input files were specified.");
		return FALSE;
	}

	if (!options->context_name || !g_ascii_strcasecmp (options->context_name, "transport"))
		options->context = GMIME_COMPLIANCE_CONTEXT_TRANSPORT;
	else if (!g_ascii_strcasecmp (options->context_name, "storage"))
		options->context = GMIME_COMPLIANCE_CONTEXT_STORAGE;
	else {
		g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Unknown context: %s", options->context_name);
		return FALSE;
	}

	if (!options->severity_name || !g_ascii_strcasecmp (options->severity_name, "minor"))
		options->minimum_severity = GMIME_COMPLIANCE_SEVERITY_MINOR;
	else if (!g_ascii_strcasecmp (options->severity_name, "major"))
		options->minimum_severity = GMIME_COMPLIANCE_SEVERITY_MAJOR;
	else if (!g_ascii_strcasecmp (options->severity_name, "critical"))
		options->minimum_severity = GMIME_COMPLIANCE_SEVERITY_CRITICAL;
	else {
		g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Unknown severity: %s", options->severity_name);
		return FALSE;
	}

	if (options->max_issues < 1 || options->max_per_violation < 0) {
		g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
				     "The issue limit must be positive and the per-violation limit must be nonnegative.");
		return FALSE;
	}

	if (!options->category_names)
		return TRUE;

	categories = g_strsplit (options->category_names, ",", -1);

	for (i = 0; categories[i]; i++) {
		char *token = g_strstrip (categories[i]);

		if (!*token)
			continue;

		if (!g_ascii_strcasecmp (token, "interoperability") || !g_ascii_strcasecmp (token, "interop"))
			options->categories |= GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY;
		else if (!g_ascii_strcasecmp (token, "dataloss"))
			options->categories |= GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;
		else if (!g_ascii_strcasecmp (token, "security"))
			options->categories |= GMIME_COMPLIANCE_CATEGORY_SECURITY;
		else {
			g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Unknown category: %s", token);
			g_strfreev (categories);
			return FALSE;
		}
	}

	g_strfreev (categories);

	if (!options->categories) {
		g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "No categories were specified.");
		return FALSE;
	}

	return TRUE;
}

int
main (int argc, char **argv)
{
	Options options = { .max_issues = 1000 };
	GOptionEntry entries[] = {
		{ "context", 'c', 0, G_OPTION_ARG_STRING, &options.context_name, "Reporting context (default: transport)", "transport|storage" },
		{ "severity", 's', 0, G_OPTION_ARG_STRING, &options.severity_name, "Minimum severity (default: minor)", "minor|major|critical" },
		{ "category", 0, 0, G_OPTION_ARG_STRING, &options.category_names, "Comma-separated categories: interoperability, dataloss, security", "LIST" },
		{ "remarks", 'r', 0, G_OPTION_ARG_NONE, &options.remarks, "Print the explanation of each issue", NULL },
		{ "werror", 'W', 0, G_OPTION_ARG_NONE, &options.werror, "Report every issue as an error", NULL },
		{ "max-issues", 0, 0, G_OPTION_ARG_INT, &options.max_issues, "Maximum collected issues (default: 1000)", "N" },
		{ "max-per-violation", 0, 0, G_OPTION_ARG_INT, &options.max_per_violation, "Maximum reports per violation (default: 0, unlimited)", "N" },
		{ "no-caret", 0, 0, G_OPTION_ARG_NONE, &options.no_caret, "Do not quote source lines", NULL },
		{ "no-color", 0, 0, G_OPTION_ARG_NONE, &options.no_color, "Disable colored output", NULL },
		{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &options.quiet, "Only print per-file summaries", NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &options.files, NULL, "FILE..." },
		{ NULL }
	};
	GOptionContext *context = g_option_context_new ("- analyze messages for MIME compliance");
	DiagnosticWriter writer = { .max_width = 100 };
	struct winsize terminal;
	GError *error = NULL;
	Counts totals = { 0 };
	gboolean failed = FALSE;
	int result = 0;
	guint i, nfiles;

	g_option_context_set_summary (context, "Report MIME compliance violations as compiler-style diagnostics.");
	g_option_context_set_description (context,
		"Exit codes:\n  0  no issue was reported as an error\n"
		"  1  an issue was reported as an error, or an input/output operation failed\n"
		"  2  the command line could not be parsed");
	g_option_context_add_main_entries (context, entries, NULL);

	if (!parse_options (context, &argc, &argv, &options, &error)) {
		char *help = g_option_context_get_help (context, TRUE, NULL);

		g_printerr ("mime-analyzer: error: %s\n\n", error->message);
		g_print ("%s", help);
		g_free (help);
		g_error_free (error);
		result = 2;
		goto done;
	}

	g_mime_init ();

	writer.color = !options.no_color && !g_getenv ("NO_COLOR") && isatty (STDOUT_FILENO);
	writer.show_source = !options.no_caret;
	writer.show_remarks = options.remarks;

	if (isatty (STDOUT_FILENO) && ioctl (STDOUT_FILENO, TIOCGWINSZ, &terminal) == 0 && terminal.ws_col > 40)
		writer.max_width = terminal.ws_col - 12;

	nfiles = g_strv_length (options.files);

	for (i = 0; i < nfiles; i++) {
		if (!analyze_file (options.files[i], &options, &writer, &totals))
			failed = TRUE;
	}

	if (nfiles > 1) {
		GString *text = g_string_new (NULL);

		append_count (text, totals.errors, "error");
		append_count (text, totals.warnings, "warning");
		append_count (text, totals.notes, "note");
		g_print ("\n%u file%s analyzed: %s\n", nfiles, nfiles == 1 ? "" : "s", text->str);
		g_string_free (text, TRUE);
	}

	result = failed || totals.errors ? 1 : 0;

	g_mime_shutdown ();

	if (fflush (stdout) == EOF || ferror (stdout)) {
		g_printerr ("mime-analyzer: error: writing output: %s\n", g_strerror (errno));
		result = 1;
	}

done:
	g_option_context_free (context);
	g_strfreev (options.files);
	g_free (options.context_name);
	g_free (options.severity_name);
	g_free (options.category_names);

	return result;
}
