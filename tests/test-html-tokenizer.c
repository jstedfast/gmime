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

#include <config.h>
#include <errno.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-html-tokenizer.h>
#include <gmime/gmime-html-utils.h>
#include <gmime/gmime-html-entity.h>
#include <gmime/gmime-html-tree-private.h>
#include "data/html-tokenizer/vectors.h"
#include "data/html-tokenizer/compliance-vectors.h"

typedef struct {
	GMimeStream parent;
	const char *input;
	gsize length, position, chunk;
	gssize fail_at;
	guint closes;
} ChunkStream;

typedef GMimeStreamClass ChunkStreamClass;

G_DEFINE_TYPE (ChunkStream, chunk_stream, GMIME_TYPE_STREAM)

static ssize_t
chunk_read (GMimeStream *stream, char *buffer, size_t count)
{
	ChunkStream *chunked = (ChunkStream *) stream;

	if (chunked->fail_at >= 0 && chunked->position >= (gsize) chunked->fail_at) {
		errno = EIO;
		return -1;
	}

	count = MIN (count, MIN (chunked->chunk, chunked->length - chunked->position));
	if (chunked->fail_at >= 0)
		count = MIN (count, (gsize) chunked->fail_at - chunked->position);

	memcpy (buffer, chunked->input + chunked->position, count);
	chunked->position += count;
	return count;
}

static gboolean
chunk_eos (GMimeStream *stream)
{
	ChunkStream *chunked = (ChunkStream *) stream;

	return chunked->position == chunked->length;
}

static int
chunk_close (GMimeStream *stream)
{
	((ChunkStream *) stream)->closes++;
	return 0;
}

static void
chunk_stream_class_init (ChunkStreamClass *klass)
{
	klass->read = chunk_read;
	klass->eos = chunk_eos;
	klass->close = chunk_close;
}

static void
chunk_stream_init (ChunkStream *chunked)
{
	chunked->fail_at = -1;
	chunked->chunk = 4096;
}

static ChunkStream *
chunk_new (const char *input, gsize length, gsize chunk)
{
	ChunkStream *chunked = g_object_new (chunk_stream_get_type (), NULL);

	chunked->input = input;
	chunked->length = length;
	chunked->chunk = chunk;
	return chunked;
}

static GMimeHtmlToken *
next (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = NULL;
	GError *error = NULL;

	g_assert_true (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &error));
	g_assert_no_error (error);
	return token;
}

static char *
describe (GMimeHtmlTokenizer *tokenizer)
{
	GString *buffer = g_string_new (NULL);
	GMimeHtmlToken *token;
	gboolean data = FALSE;

	while ((token = next (tokenizer)) != NULL) {
		GMimeHtmlTokenKind kind = g_mime_html_token_get_kind (token);
		gboolean isdata = kind == GMIME_HTML_TOKEN_DATA || kind == GMIME_HTML_TOKEN_CDATA ||
				  kind == GMIME_HTML_TOKEN_SCRIPT_DATA;
		gboolean merge = data && isdata;

		if (merge)
			g_string_truncate (buffer, buffer->len - 1);
		else if (buffer->len)
			g_string_append_c (buffer, ' ');

		if (kind == GMIME_HTML_TOKEN_TAG) {
			g_string_append (buffer, g_mime_html_token_is_end_tag (token) ? "EndTag(" : "Tag(");
			g_string_append (buffer, g_mime_html_token_get_name (token));
			for (gsize i = 0; i < g_mime_html_token_get_n_attributes (token); i++) {
				const GMimeHtmlAttribute *attribute = g_mime_html_token_get_attribute (token, i);

				g_string_append_c (buffer, ' ');
				g_string_append (buffer, g_mime_html_attribute_get_name (attribute));
				if (g_mime_html_attribute_get_value (attribute))
					g_string_append_printf (buffer, "=\"%s\"",
								g_mime_html_attribute_get_value (attribute));
			}

			g_string_append (buffer, g_mime_html_token_is_empty_element (token) ? "/)" : ")");
		} else if (kind == GMIME_HTML_TOKEN_COMMENT) {
			g_string_append (buffer,
					 g_mime_html_token_get_is_bogus_comment (token) ? "Bogus(" : "Comment(");
			g_string_append (buffer, g_mime_html_token_get_data (token));
			g_string_append_c (buffer, ')');
		} else if (isdata) {
			if (!merge)
				g_string_append (buffer, "Data(");

			g_string_append_len (buffer, g_mime_html_token_get_data (token),
					     g_mime_html_token_get_data_length (token));
			g_string_append_c (buffer, ')');
		} else
			g_string_append (buffer, "DocType");
		data = isdata;
		g_mime_html_token_unref (token);
	}

	return g_string_free (buffer, FALSE);
}

static void
test_vectors (void)
{
	static const gsize chunks[] = { 1, 2, 7, 4096 };

	for (gsize i = 0; i < G_N_ELEMENTS (vectors); i++) {
		for (gsize c = 0; c < G_N_ELEMENTS (chunks); c++) {
			ChunkStream *stream = chunk_new (vectors[i].input, strlen (vectors[i].input), chunks[c]);
			GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
			char *actual;

			g_mime_html_tokenizer_set_decode_character_references (tokenizer, vectors[i].decode);
			g_mime_html_tokenizer_set_scripting_enabled (tokenizer, vectors[i].scripting);
			g_mime_html_tokenizer_set_max_element_depth (tokenizer, vectors[i].depth);
			actual = describe (tokenizer);
			if (vectors[i].suffix ? !g_str_has_suffix (actual, vectors[i].expected) :
						strcmp (actual, vectors[i].expected))
				g_error ("%s vector %" G_GSIZE_FORMAT ", chunk %" G_GSIZE_FORMAT
					 "\ninput: %s\nexpected: %s\nactual: %s",
					 vectors[i].label, i, chunks[c], vectors[i].input, vectors[i].expected, actual);
			g_assert_cmpint (g_mime_html_tokenizer_get_state (tokenizer), ==,
					 GMIME_HTML_TOKENIZER_STATE_END_OF_FILE);
			g_free (actual);
			g_object_unref (tokenizer);
			g_assert_cmpuint (stream->closes, ==, 0);
			g_object_unref (stream);
		}
	}
}

static void
test_entities (void)
{
	static const struct {
		const char *input, *value;
	} numeric[] = { { "&#0;", "\357\277\275" },
			{ "&#x80;", "\342\202\254" },
			{ "&#xD800;", "\357\277\275" },
			{ "&#1114112;", "\357\277\275" },
			{ "&#999999999999999999999999999999999999999999999999999999999999;", "\357\277\275" },
			{ "&#x;", "&#x;" },
			{ "&notit;", "\302\254it;" },
			{ "&notin;", "\342\210\211" },
			{ "&#x1F600;", "\360\237\230\200" },
			{ "&fjlig;", "fj" } };

	for (gsize i = 0; i < G_N_ELEMENTS (entities); i++) {
		char *actual = g_mime_html_decode (entities[i].name);

		g_assert_cmpstr (actual, ==, entities[i].value);
		g_free (actual);
	}

	GMimeHtmlEntityDecoder *decoder = g_mime_html_entity_decoder_new ();

	g_assert_cmpstr (g_mime_html_entity_decoder_get_value (decoder), ==, "");
	for (gsize i = 0; i < G_N_ELEMENTS (entities); i++) {
		g_mime_html_entity_decoder_reset (decoder);
		for (const char *p = entities[i].name; *p; p++)
			g_assert_true (g_mime_html_entity_decoder_push (decoder, *p));

		g_assert_cmpstr (g_mime_html_entity_decoder_get_value (decoder), ==, entities[i].value);
		g_assert_false (g_mime_html_entity_decoder_push (decoder, ' '));
	}

	g_object_unref (decoder);
	for (gsize i = 0; i < G_N_ELEMENTS (numeric); i++) {
		char *actual = g_mime_html_decode (numeric[i].input);

		g_assert_cmpstr (actual, ==, numeric[i].value);
		g_free (actual);
	}

	char *actual = g_mime_html_attribute_decode ("&notit; &not= &amp; &not;");

	g_assert_cmpstr (actual, ==, "&notit; &not= & \302\254");
	g_free (actual);
}

static void
append_quote (GString *buffer, const char *text)
{
	g_string_append_c (buffer, '"');
	for (const char *p = text; *p; p++) {
		if (*p == '\r')
			continue;

		if (*p == '\\' || *p == '"')
			g_string_append_c (buffer, '\\');

		g_string_append_c (buffer, *p);
	}

	g_string_append_c (buffer, '"');
}

static void
append_fixture_token (GString *buffer, GMimeHtmlToken *token)
{
	static const char *kinds[] = { "Data", "Tag", "Comment", "DocType", "CData", "ScriptData" };
	GMimeHtmlTokenKind kind = g_mime_html_token_get_kind (token);

	g_string_append_printf (buffer, "%s: ", kinds[kind]);
	if (kind == GMIME_HTML_TOKEN_TAG) {
		g_string_append (buffer, g_mime_html_token_is_end_tag (token) ? "</" : "<");
		g_string_append (buffer, g_mime_html_token_get_name (token));
		for (gsize i = 0; i < g_mime_html_token_get_n_attributes (token); i++) {
			const GMimeHtmlAttribute *attribute = g_mime_html_token_get_attribute (token, i);

			g_string_append_c (buffer, ' ');
			g_string_append (buffer, g_mime_html_attribute_get_name (attribute));
			if (g_mime_html_attribute_get_value (attribute)) {
				g_string_append_c (buffer, '=');
				append_quote (buffer, g_mime_html_attribute_get_value (attribute));
			}
		}

		g_string_append (buffer, g_mime_html_token_is_empty_element (token) ? "/>" : ">");
	} else if (kind == GMIME_HTML_TOKEN_DOCTYPE) {
		const char *name = g_mime_html_token_get_name (token),
			   *pub = g_mime_html_token_get_public_identifier (token),
			   *sys = g_mime_html_token_get_system_identifier (token);

		if (g_mime_html_token_get_force_quirks_mode (token))
			g_string_append (buffer, "<!-- force quirks mode -->");

		g_string_append (buffer, "<!DOCTYPE");
		if (name) {
			char *upper = g_ascii_strup (name, -1);

			g_string_append_c (buffer, ' ');
			g_string_append (buffer, upper);
			g_free (upper);
		}

		if (pub) {
			g_string_append (buffer, " PUBLIC ");
			append_quote (buffer, pub);
			if (sys) {
				g_string_append_c (buffer, ' ');
				append_quote (buffer, sys);
			}
		} else if (sys) {
			g_string_append (buffer, " SYSTEM ");
			append_quote (buffer, sys);
		}

		g_string_append_c (buffer, '>');
	} else {
		const char *data = g_mime_html_token_get_data (token);

		for (gsize i = 0; i < g_mime_html_token_get_data_length (token); i++) {
			if (data[i] == '\r')
				continue;

			if (kind != GMIME_HTML_TOKEN_COMMENT && data[i] == '\n')
				g_string_append (buffer, "\\n");
			else if (kind != GMIME_HTML_TOKEN_COMMENT && data[i] == '\t')
				g_string_append (buffer, "\\t");
			else if (kind != GMIME_HTML_TOKEN_COMMENT && data[i] == '\f')
				g_string_append (buffer, "\\f");
			else
				g_string_append_c (buffer, data[i]);
		}
	}

	g_string_append_c (buffer, '\n');
}

static const char *fixture_root;

static char *
fixture (const char *name, const char *suffix, gsize *len)
{
	char *path = g_strdup_printf ("%s/%s%s", fixture_root, name, suffix), *text;
	GError *error = NULL;
	gboolean success = g_file_get_contents (path, &text, len, &error);

	g_assert_no_error (error);
	g_assert_true (success);
	g_free (path);
	return text;
}

static void
test_corpus (void)
{
	static const char *names[] = { "badly-quoted-attr", "blocked",     "cdata", "papercut-4.4", "papercut",
				       "plaintext",         "script-data", "test",  "xamarin3" };

	for (gsize n = 0; n < G_N_ELEMENTS (names); n++) {
		gsize length;
		char *input = fixture (names[n], ".html", &length);
		char *expected = fixture (names[n], ".out.html", NULL),
		     *expected_tokens = fixture (names[n], ".tokens", NULL);

		for (gsize chunk = 1; chunk <= 4096; chunk *= 64) {
			ChunkStream *stream = chunk_new (input, length, chunk);
			GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
			GMimeHtmlToken *token;
			GString *output = g_string_new (NULL), *tokens = g_string_new (NULL);

			while ((token = next (tokenizer)) != NULL) {
				char *value = g_mime_html_token_to_string (token);

				g_string_append (output, value);
				g_free (value);
				append_fixture_token (tokens, token);
				g_mime_html_token_unref (token);
			}

			if (strcmp (output->str, expected) || strcmp (tokens->str, expected_tokens)) {
				gsize i = 0;

				while (i < tokens->len && expected_tokens[i] && tokens->str[i] == expected_tokens[i])
					i++;

				g_error ("Corpus %s chunk %" G_GSIZE_FORMAT " mismatch at %" G_GSIZE_FORMAT
					 "\nexpected: %.160s\nactual: %.160s\nserialization matches: %d",
					 names[n], chunk, i, expected_tokens + i, tokens->str + i,
					 !strcmp (output->str, expected));
			}

			g_string_free (output, TRUE);
			g_string_free (tokens, TRUE);
			g_object_unref (tokenizer);
			g_object_unref (stream);
		}

		g_free (input);
		g_free (expected);
		g_free (expected_tokens);
	}
}

static void
log_issue (const GMimeHtmlComplianceIssue *issue, gpointer data)
{
	g_array_append_val ((GArray *) data, *issue);
}

static void
test_compliance_vectors (void)
{
	for (gsize i = 0; i < G_N_ELEMENTS (compliance_vectors); i++) {
		const ComplianceVector *vector = &compliance_vectors[i];

		for (gsize chunk = 1; chunk <= 4096; chunk *= 4096) {
			for (guint decode = 0; decode < 2; decode++) {
				ChunkStream *stream = chunk_new (vector->input, vector->length, chunk);
				GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
				GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue));
				GMimeHtmlToken *token;

				g_mime_html_tokenizer_set_decode_character_references (tokenizer, decode);
				g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_issue, issues, NULL);
				while ((token = next (tokenizer)) != NULL)
					g_mime_html_token_unref (token);

				if (issues->len != vector->count)
					g_error ("Compliance vector %" G_GSIZE_FORMAT
						 " (%s), decode %u: expected %" G_GSIZE_FORMAT " issues, got %u",
						 i, vector->input, decode, vector->count, issues->len);
				for (guint n = 0; n < issues->len; n++) {
					GMimeHtmlComplianceIssue *actual =
						&g_array_index (issues, GMimeHtmlComplianceIssue, n);

					if (actual->violation != vector->issues[n].violation ||
					    actual->offset != vector->issues[n].offset ||
					    actual->line_number != vector->issues[n].line ||
					    actual->line_position != vector->issues[n].position)
						g_error ("Compliance vector %" G_GSIZE_FORMAT
							 " (%s), issue %u: expected %d at %d:%d (%" G_GINT64_FORMAT
							 "), got %d at %d:%d (%" G_GINT64_FORMAT ")",
							 i, vector->input, n, vector->issues[n].violation,
							 vector->issues[n].line, vector->issues[n].position,
							 vector->issues[n].offset, actual->violation,
							 actual->line_number, actual->line_position, actual->offset);
				}

				g_array_unref (issues);
				g_object_unref (tokenizer);
				g_object_unref (stream);
			}
		}
	}
}

static void
test_compliance (void)
{
	const char *input = "<a X=1 x=2>&#0;&bogus;<!--";
	ChunkStream *stream = chunk_new (input, strlen (input), 1);
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue));
	GMimeHtmlToken *token;

	g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_issue, issues, NULL);
	while ((token = next (tokenizer)) != NULL)
		g_mime_html_token_unref (token);

	g_assert_cmpuint (issues->len, ==, 4);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 0).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 0).offset, ==, 7);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 1).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 2).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 3).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_EOF_IN_COMMENT);
	g_array_unref (issues);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
test_compliance_cap (void)
{
	const char *input = "&#0;&#0;&#0;&bogus;&bogus;&bogus;";
	ChunkStream *stream = chunk_new (input, strlen (input), 1);
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	GArray *issues = g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue));
	GMimeHtmlToken *token;

	g_mime_html_compliance_options_set_max_issues_per_violation (g_mime_html_tokenizer_get_compliance_options (
									     tokenizer),
								     1);
	g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_issue, issues, NULL);
	while ((token = next (tokenizer)) != NULL)
		g_mime_html_token_unref (token);

	g_assert_cmpuint (issues->len, ==, 3);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 0).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 1).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 1).offset, ==, 4);
	g_assert_cmpint (g_array_index (issues, GMimeHtmlComplianceIssue, 2).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_UNKNOWN_NAMED_CHARACTER_REFERENCE);
	g_array_unref (issues);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
read_duplicate_tag (GMimeHtmlTokenizer *tokenizer)
{
	GMimeHtmlToken *token = next (tokenizer);

	g_assert_nonnull (token);
	g_mime_html_token_unref (token);
}

typedef struct {
	GArray *issues;
	guint destroyed;
} ComplianceLog;

static void
log_tracked_issue (const GMimeHtmlComplianceIssue *issue, gpointer data)
{
	log_issue (issue, ((ComplianceLog *) data)->issues);
}

static void
destroy_compliance_log (gpointer data)
{
	((ComplianceLog *) data)->destroyed++;
}

static void
test_compliance_budget_changes (void)
{
	GMimeHtmlComplianceOptions *options = g_mime_html_compliance_options_new (), *clone;
	ComplianceLog first = { g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue)), 0 };
	ComplianceLog second = { g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue)), 0 };
	ComplianceLog last = { g_array_new (FALSE, FALSE, sizeof (GMimeHtmlComplianceIssue)), 0 };
	GString *input = g_string_new (NULL);
	GMimeHtmlTokenizer *tokenizer;
	ChunkStream *stream;
	guint i;

	for (i = 0; i < 17; i++)
		g_string_append (input, "<p x=1 x=2>");
	stream = chunk_new (input->str, input->len, 1);
	tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	g_mime_html_compliance_options_set_max_issues_per_violation (options, 1);
	g_mime_html_tokenizer_set_compliance_options (tokenizer, options);
	g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_tracked_issue, &first, destroy_compliance_log);
	for (i = 0; i < 3; i++)
		read_duplicate_tag (tokenizer);
	g_assert_cmpuint (first.issues->len, ==, 2);

	g_mime_html_compliance_options_set_max_issues_per_violation (options, 2);
	for (i = 0; i < 3; i++)
		read_duplicate_tag (tokenizer);
	g_assert_cmpuint (first.issues->len, ==, 5);

	clone = g_mime_html_compliance_options_clone (options);
	g_mime_html_compliance_options_set_max_issues_per_violation (options, 0);
	g_mime_html_tokenizer_set_compliance_options (tokenizer, clone);
	g_mime_html_compliance_options_unref (clone);
	for (i = 0; i < 3; i++)
		read_duplicate_tag (tokenizer);
	g_assert_cmpuint (first.issues->len, ==, 8);
	for (i = 0; i < first.issues->len; i++) {
		GMimeHtmlComplianceIssue *issue = &g_array_index (first.issues, GMimeHtmlComplianceIssue, i);
		gboolean truncated = i == 1 || i == 4 || i == 7;

		g_assert_cmpint (issue->violation, ==, truncated ?
				 GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES :
				 GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE);
		g_assert_cmpint (issue->severity, ==, GMIME_COMPLIANCE_SEVERITY_MAJOR);
		g_assert_cmpint (issue->categories, ==, truncated ? GMIME_COMPLIANCE_CATEGORY_SECURITY :
				 GMIME_COMPLIANCE_CATEGORY_SECURITY | GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY);
	}

	g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_tracked_issue, &second, destroy_compliance_log);
	g_assert_cmpuint (first.destroyed, ==, 1);
	for (i = 0; i < 3; i++)
		read_duplicate_tag (tokenizer);
	g_assert_cmpuint (second.issues->len, ==, 3);
	g_assert_cmpint (g_array_index (second.issues, GMimeHtmlComplianceIssue, 2).violation, ==,
			 GMIME_HTML_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES);

	g_mime_html_tokenizer_set_compliance_options (tokenizer, options);
	g_mime_html_compliance_options_unref (options);
	for (i = 0; i < 3; i++)
		read_duplicate_tag (tokenizer);
	g_assert_cmpuint (second.issues->len, ==, 6);
	for (i = 3; i < second.issues->len; i++)
		g_assert_cmpint (g_array_index (second.issues, GMimeHtmlComplianceIssue, i).violation, ==,
				 GMIME_HTML_COMPLIANCE_VIOLATION_DUPLICATE_ATTRIBUTE);

	g_mime_html_tokenizer_set_compliance_logger (tokenizer, NULL, NULL, NULL);
	g_assert_cmpuint (second.destroyed, ==, 1);
	read_duplicate_tag (tokenizer);
	g_assert_cmpuint (second.issues->len, ==, 6);
	g_mime_html_tokenizer_set_compliance_logger (tokenizer, log_tracked_issue, &last, destroy_compliance_log);
	read_duplicate_tag (tokenizer);
	g_assert_cmpuint (last.issues->len, ==, 1);
	g_assert_null (next (tokenizer));
	g_object_unref (tokenizer);
	g_assert_cmpuint (first.destroyed, ==, 1);
	g_assert_cmpuint (second.destroyed, ==, 1);
	g_assert_cmpuint (last.destroyed, ==, 1);
	g_object_unref (stream);
	g_array_unref (first.issues);
	g_array_unref (second.issues);
	g_array_unref (last.issues);
	g_string_free (input, TRUE);
}

static void
test_errors (void)
{
	static const char *bad[] = { "\xc3", "\xc0\x80", "\xf4\x90\x80\x80", "<p>\xed\xa0\x80", "good\xf0\x9f" };

	for (gsize i = 0; i < G_N_ELEMENTS (bad); i++) {
		ChunkStream *stream = chunk_new (bad[i], strlen (bad[i]), 1);
		GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
		GError *error = NULL;
		GMimeHtmlToken *token = NULL;

		while (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &error) && token) {
			g_mime_html_token_unref (token);
			token = NULL;
		}

		g_assert_nonnull (error);
		g_assert_null (token);
		g_clear_error (&error);
		g_object_unref (tokenizer);
		g_assert_cmpuint (stream->closes, ==, 0);
		g_object_unref (stream);
	}

	ChunkStream *stream = chunk_new ("<tag value=truncated", 20, 1);

	stream->fail_at = 10;
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	GError *error = NULL;
	GMimeHtmlToken *token;

	g_assert_false (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &error));
	g_assert_error (error, GMIME_ERROR, EIO);
	g_clear_error (&error);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
test_lifetime_and_limits (void)
{
	const char *input = "one<b>two<i title='x";
	ChunkStream *stream = chunk_new (input, strlen (input), 2);
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	GMimeHtmlToken *first = next (tokenizer), *token;

	g_mime_html_tokenizer_set_ignore_truncated_tags (tokenizer, TRUE);
	while ((token = next (tokenizer)) != NULL)
		g_mime_html_token_unref (token);

	g_assert_cmpstr (g_mime_html_token_get_data (first), ==, "one");
	g_object_unref (tokenizer);
	g_assert_cmpstr (g_mime_html_token_get_data (first), ==, "one");
	g_mime_html_token_unref (first);
	g_object_unref (stream);
	stream = chunk_new ("<tag a='very long truncated", 26, 1);
	tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	_g_mime_html_tokenizer_set_max_input_length (tokenizer, 10);
	GError *error = NULL;

	g_assert_false (g_mime_html_tokenizer_read_next_token (tokenizer, &token, &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
test_charset (void)
{
	static const char utf16[] = "\xff\xfe<\0p\0>\0x\0";
	ChunkStream *stream = chunk_new (utf16, sizeof utf16 - 1, 1);
	GError *error = NULL;
	GMimeHtmlTokenizer *tokenizer =
		g_mime_html_tokenizer_new_with_charset (GMIME_STREAM (stream), "UTF-8", TRUE, 1, &error);

	g_assert_no_error (error);
	g_assert_nonnull (tokenizer);
	char *actual = describe (tokenizer);

	g_assert_cmpstr (actual, ==, "Tag(p) Data(x)");
	g_free (actual);
	g_object_unref (tokenizer);
	g_assert_cmpuint (stream->closes, ==, 0);
	g_object_unref (stream);
	stream = chunk_new ("\xef\xbb\xbfX", 4, 1);
	tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	GMimeHtmlToken *token = next (tokenizer);

	g_assert_cmpstr (g_mime_html_token_get_data (token), ==, "\xef\xbb\xbfX");
	g_mime_html_token_unref (token);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
test_serialization (void)
{
	static const struct {
		const char *input, *expected;
	} cases[] = { { "<a/>", "<a/>" },
		      { "<a x =y>", "<a x=\"y\">" },
		      { "<a x =y =z>", "<a x=\"y\" =z>" },
		      { "<a x =\"y\" =z>", "<a x=\"y\" =z>" },
		      { "<<!--x-->img>", "&lt;<!--x-->img>" },
		      { "<</>c>", "&lt;c>" },
		      { "</<o", "</<o>" },
		      { "</\303\251>", "</\303\251>" },
		      { "<!dOcTyPe html PuBlIc 'a\"b' \"c'd\">", "<!dOcTyPe html PuBlIc 'a\"b' \"c'd\">" },
		      { "<a title='unfinished", "<a title='unfinished" },
		      { "<a ", "<a " },
		      { "<a x ", "<a x " },
		      { "<a x=", "<a x=" },
		      { "<a x=foo ", "<a x=foo " } };

	for (gsize i = 0; i < G_N_ELEMENTS (cases); i++) {
		ChunkStream *stream = chunk_new (cases[i].input, strlen (cases[i].input), 1);
		GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
		GMimeHtmlToken *token;
		GString *builder = g_string_new (NULL);

		g_mime_html_tokenizer_set_decode_character_references (tokenizer, FALSE);
		while ((token = next (tokenizer)) != NULL) {
			char *text = g_mime_html_token_to_string (token);

			g_string_append (builder, text);
			g_free (text);
			g_mime_html_token_unref (token);
		}

		g_assert_cmpstr (builder->str, ==, cases[i].expected);
		g_string_free (builder, TRUE);
		g_object_unref (tokenizer);
		g_object_unref (stream);
	}

	GMimeHtmlToken *token = g_mime_html_token_new_data ("<b>&copy;</b>");
	char *text = g_mime_html_token_to_string (token);

	g_assert_cmpstr (text, ==, "<b>&copy;</b>");
	g_free (text);
	g_mime_html_token_unref (token);
	GMimeHtmlAttribute *attributes[] = { g_mime_html_attribute_new ("x", NULL),
					     g_mime_html_attribute_new ("=z", NULL) };

	token = g_mime_html_token_new_tag ("a", FALSE, FALSE, attributes, G_N_ELEMENTS (attributes));
	text = g_mime_html_token_to_string (token);
	g_assert_cmpstr (text, ==, "<a x=\"\" =z>");
	g_free (text);
	g_mime_html_token_unref (token);
	g_mime_html_attribute_unref (attributes[0]);
	g_mime_html_attribute_unref (attributes[1]);
	ChunkStream *stream = chunk_new ("a\0b", 3, 1);
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));

	g_mime_html_tokenizer_set_decode_character_references (tokenizer, FALSE);
	token = next (tokenizer);
	g_assert_cmpuint (g_mime_html_token_get_data_length (token), ==, 3);
	g_assert_cmpmem (g_mime_html_token_get_data (token), 3, "a\0b", 3);
	GMimeStream *output = g_mime_stream_mem_new ();
	GError *error = NULL;

	g_assert_true (g_mime_html_token_write_to (token, output, &error));
	g_assert_no_error (error);
	GByteArray *buffer = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));

	g_assert_cmpmem (buffer->data, buffer->len, "a\0b", 3);
	g_object_unref (output);
	g_mime_html_token_unref (token);
	g_object_unref (tokenizer);
	g_object_unref (stream);
	stream = chunk_new ("</\0>", 4, 1);
	tokenizer = g_mime_html_tokenizer_new (GMIME_STREAM (stream));
	token = next (tokenizer);
	g_assert_cmpint (g_mime_html_token_get_kind (token), ==, GMIME_HTML_TOKEN_COMMENT);
	g_assert_cmpstr (g_mime_html_token_get_data (token), ==, "\357\277\275");
	text = g_mime_html_token_to_string (token);
	g_assert_cmpstr (text, ==, "</\357\277\275>");
	g_free (text);
	g_mime_html_token_unref (token);
	g_object_unref (tokenizer);
	g_object_unref (stream);
}

static void
test_configuration_validation (void)
{
	const GMimeHtmlNamespace invalid_namespaces[] = { (GMimeHtmlNamespace) -1,
							  (GMimeHtmlNamespace) (GMIME_HTML_NAMESPACE_XML_NS + 1) };
	const guint invalid_depths[] = { 0, G_MAXUINT };
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeHtmlTokenizer *tokenizer = g_mime_html_tokenizer_new (stream);

	g_mime_html_tokenizer_set_namespace (tokenizer, GMIME_HTML_NAMESPACE_SVG);
	for (guint i = 0; i < G_N_ELEMENTS (invalid_namespaces); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*ns >= GMIME_HTML_NAMESPACE_HTML*");
		g_mime_html_tokenizer_set_namespace (tokenizer, invalid_namespaces[i]);
		g_test_assert_expected_messages ();
		g_assert_cmpint (g_mime_html_tokenizer_get_namespace (tokenizer), ==, GMIME_HTML_NAMESPACE_SVG);
	}

	g_mime_html_tokenizer_set_max_element_depth (tokenizer, 17);
	for (guint i = 0; i < G_N_ELEMENTS (invalid_depths); i++) {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*maximum > 0 && maximum <= G_MAXINT*");
		g_mime_html_tokenizer_set_max_element_depth (tokenizer, invalid_depths[i]);
		g_test_assert_expected_messages ();
		g_assert_cmpuint (g_mime_html_tokenizer_get_max_element_depth (tokenizer), ==, 17);
	}

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_HTML_TOKENIZER (tokenizer)*");
	g_mime_html_tokenizer_set_scripting_enabled (NULL, TRUE);
	g_test_assert_expected_messages ();

	g_object_unref (tokenizer);
	g_object_unref (stream);
}

int
main (int argc, char **argv)
{
	/* The GMime test runner uses -v, but GTest only consumes --verbose. */
	for (int i = 1; i < argc; i++)
		if (!strcmp (argv[i], "-v"))
			argv[i] = (char *) "--verbose";

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	fixture_root = g_getenv ("GMIME_HTML_FIXTURES");
	if (!fixture_root)
		fixture_root = argc > 1 ? argv[1] : "tests/data/html-tokenizer";

	g_test_add_func ("/html/tokenizer/upstream-vectors", test_vectors);
	g_test_add_func ("/html/tokenizer/upstream-corpus", test_corpus);
	g_test_add_func ("/html/entities/upstream", test_entities);
	g_test_add_func ("/html/tokenizer/compliance", test_compliance);
	g_test_add_func ("/html/tokenizer/upstream-compliance", test_compliance_vectors);
	g_test_add_func ("/html/tokenizer/compliance-cap", test_compliance_cap);
	g_test_add_func ("/html/tokenizer/compliance-budget-changes", test_compliance_budget_changes);
	g_test_add_func ("/html/tokenizer/errors", test_errors);
	g_test_add_func ("/html/tokenizer/lifetime-limits", test_lifetime_and_limits);
	g_test_add_func ("/html/tokenizer/charset", test_charset);
	g_test_add_func ("/html/tokenizer/serialization", test_serialization);
	g_test_add_func ("/html/tokenizer/configuration-validation", test_configuration_validation);
	int result = g_test_run ();

	g_mime_shutdown ();
	return result;
}
