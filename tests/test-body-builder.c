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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gmime/gmime.h>

#include "testsuite.h"

static void
check_text (GMimeEntity *entity, const char *subtype, const char *charset, const char *text)
{
	char *value;

	g_assert_true (GMIME_IS_TEXT_PART (entity));
	g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", subtype));
	g_assert_cmpstr (g_mime_text_part_get_charset (GMIME_TEXT_PART (entity)), ==, charset);
	g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_assert_null (g_mime_entity_get_header (entity, "Content-Transfer-Encoding"));
	value = g_mime_text_part_get_text (GMIME_TEXT_PART (entity));
	g_assert_cmpstr (value, ==, text);
	g_free (value);
}

static void
check_bytes (GMimeEntity *entity, const guint8 *expected, gsize length)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	GByteArray *bytes;

	g_assert_cmpint (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (entity)), output), ==, length);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	g_assert_cmpuint (bytes->len, ==, length);
	if (length)
		g_assert_cmpint (memcmp (bytes->data, expected, length), ==, 0);
	g_object_unref (output);
}

static void
test_defaults_and_strings (void)
{
	GMimeBodyBuilder *builder = g_mime_body_builder_new ();
	GMimeAttachmentCollection *attachments, *resources;
	GMimeEntity *body;
	GError *error = NULL;
	char text[] = "plain", html[] = "<html>";

	attachments = g_mime_body_builder_get_attachments (builder);
	resources = g_mime_body_builder_get_linked_resources (builder);
	g_assert_true (attachments == g_mime_body_builder_get_attachments (builder));
	g_assert_true (resources == g_mime_body_builder_get_linked_resources (builder));
	g_assert_true (attachments != resources);
	g_assert_cmpint (g_mime_attachment_collection_get_count (attachments), ==, 0);
	g_assert_cmpint (g_mime_attachment_collection_get_count (resources), ==, 0);
	g_assert_null (g_mime_body_builder_get_text_body (builder));
	g_assert_null (g_mime_body_builder_get_html_body (builder));
	g_assert_cmpstr (g_mime_body_builder_get_body_encoding (builder), ==, "utf-8");
	body = g_mime_body_builder_to_message_body (builder, &error);
	g_assert_no_error (error);
	check_text (body, "plain", "utf-8", "");
	g_object_unref (body);
	g_mime_body_builder_set_text_body (builder, text);
	g_mime_body_builder_set_html_body (builder, html);
	text[0] = 'X';
	html[0] = 'X';
	g_assert_cmpstr (g_mime_body_builder_get_text_body (builder), ==, "plain");
	g_assert_cmpstr (g_mime_body_builder_get_html_body (builder), ==, "<html>");
	g_mime_body_builder_set_text_body (builder, g_mime_body_builder_get_text_body (builder));
	g_mime_body_builder_set_html_body (builder, g_mime_body_builder_get_html_body (builder) + 1);
	g_assert_cmpstr (g_mime_body_builder_get_text_body (builder), ==, "plain");
	g_assert_cmpstr (g_mime_body_builder_get_html_body (builder), ==, "html>");
	g_mime_body_builder_set_text_body (builder, "");
	g_assert_nonnull (g_mime_body_builder_get_text_body (builder));
	g_mime_body_builder_set_html_body (builder, NULL);
	g_assert_null (g_mime_body_builder_get_html_body (builder));
	g_object_unref (builder);
}

static void
check_tree (GMimeEntity *body, const char *text, const char *html, GMimeEntity **resources, int nresources,
	    GMimeEntity **attachments, int nattachments)
{
	GMimeMultipart *multipart;
	GMimeEntity *html_part;
	int i;

	if (nattachments > 0 && !text && !html && nattachments == 1) {
		g_assert_true (body == attachments[0]);
		return;
	}
	if (nattachments > 0) {
		g_assert_true (GMIME_IS_MULTIPART (body));
		g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (body), "multipart", "mixed"));
		g_assert_nonnull (g_mime_entity_get_content_type_parameter (body, "boundary"));
		multipart = GMIME_MULTIPART (body);
		g_assert_cmpint (g_mime_multipart_get_count (multipart), ==, nattachments + (text || html ? 1 : 0));
		for (i = 0; i < nattachments; i++)
			g_assert_true (g_mime_multipart_get_part (multipart, i + (text || html ? 1 : 0)) == attachments[i]);
		if (!text && !html)
			return;
		body = g_mime_multipart_get_part (multipart, 0);
	}
	if (text && html) {
		g_assert_true (GMIME_IS_MULTIPART_ALTERNATIVE (body));
		g_assert_nonnull (g_mime_entity_get_content_type_parameter (body, "boundary"));
		g_assert_cmpint (g_mime_multipart_get_count (GMIME_MULTIPART (body)), ==, 2);
		check_text (g_mime_multipart_get_part (GMIME_MULTIPART (body), 0), "plain", "utf-8", text);
		html_part = g_mime_multipart_get_part (GMIME_MULTIPART (body), 1);
	} else {
		html_part = body;
	}
	if (html) {
		if (nresources > 0) {
			g_assert_true (GMIME_IS_MULTIPART_RELATED (html_part));
			g_assert_nonnull (g_mime_entity_get_content_type_parameter (html_part, "boundary"));
			multipart = GMIME_MULTIPART (html_part);
			g_assert_cmpint (g_mime_multipart_get_count (multipart), ==, nresources + 1);
			g_assert_cmpstr (g_mime_entity_get_content_type_parameter (html_part, "type"), ==, "text/html");
			g_assert_null (g_mime_entity_get_content_type_parameter (html_part, "start"));
			for (i = 0; i < nresources; i++)
				g_assert_true (g_mime_multipart_get_part (multipart, i + 1) == resources[i]);
			g_assert_true (g_mime_multipart_related_get_root (GMIME_MULTIPART_RELATED (html_part)) ==
				       g_mime_multipart_get_part (multipart, 0));
			html_part = g_mime_multipart_get_part (multipart, 0);
		}
		check_text (html_part, "html", "utf-8", html);
		g_assert_null (g_mime_entity_get_content_id (html_part));
	} else {
		check_text (body, "plain", "utf-8", text ? text : "");
	}
}

static void
test_structures (void)
{
	static const char *text[] = { NULL, "", "plain\r\ncaf\303\251" };
	static const char *html[] = { NULL, "", "<b>caf\303\251</b>" };
	GMimeBodyBuilder *builder;
	GMimeAttachmentCollection *attachments, *resources;
	GMimeEntity *attachment[2], *resource[2], *first, *second;
	GError *error = NULL;
	char *before_attachment, *before_resource, *after;
	guint t, h, a, r, duplicate, i;

	for (duplicate = 0; duplicate < 2; duplicate++) {
		for (t = 0; t < G_N_ELEMENTS (text); t++) {
			for (h = 0; h < G_N_ELEMENTS (html); h++) {
				for (a = 0; a <= 2; a++) {
					for (r = 0; r <= 2; r++) {
						builder = g_mime_body_builder_new ();
						attachments = g_mime_body_builder_get_attachments (builder);
						resources = g_mime_body_builder_get_linked_resources (builder);
						g_mime_body_builder_set_text_body (builder, text[t]);
						g_mime_body_builder_set_html_body (builder, html[h]);
						attachment[0] = (GMimeEntity *) g_mime_part_new ();
						resource[0] = (GMimeEntity *) g_mime_part_new ();
						attachment[1] = duplicate ? g_object_ref (attachment[0]) : (GMimeEntity *) g_mime_part_new ();
						resource[1] = duplicate ? g_object_ref (resource[0]) : (GMimeEntity *) g_mime_part_new ();
						g_mime_entity_set_header (attachment[0], "X-Test", "attachment", NULL);
						g_mime_entity_set_header (resource[0], "X-Test", "resource", NULL);
						before_attachment = g_mime_entity_get_headers (attachment[0], NULL);
						before_resource = g_mime_entity_get_headers (resource[0], NULL);
						for (i = 0; i < a; i++)
							g_mime_attachment_collection_add (attachments, attachment[i]);
						for (i = 0; i < r; i++)
							g_mime_attachment_collection_add (resources, resource[i]);
						first = g_mime_body_builder_to_message_body (builder, &error);
						g_assert_no_error (error);
						second = g_mime_body_builder_to_message_body (builder, &error);
						g_assert_no_error (error);
						check_tree (first, text[t], html[h], resource, r, attachment, a);
						check_tree (second, text[t], html[h], resource, r, attachment, a);
						g_assert_true (first != second || (!text[t] && !html[h] && a == 1));
						g_assert_cmpint (g_mime_attachment_collection_get_count (attachments), ==, a);
						g_assert_cmpint (g_mime_attachment_collection_get_count (resources), ==, r);
						after = g_mime_entity_get_headers (attachment[0], NULL);
						g_assert_cmpstr (after, ==, before_attachment);
						g_free (after);
						after = g_mime_entity_get_headers (resource[0], NULL);
						g_assert_cmpstr (after, ==, before_resource);
						g_free (after);
						g_free (before_attachment);
						g_free (before_resource);
						g_mime_attachment_collection_clear (attachments);
						g_mime_attachment_collection_clear (resources);
						g_object_unref (builder);
						check_tree (first, text[t], html[h], resource, r, attachment, a);
						g_object_unref (first);
						g_object_unref (second);
						for (i = 0; i < 2; i++) {
							g_object_unref (attachment[i]);
							g_object_unref (resource[i]);
						}
					}
				}
			}
		}
	}
}

static void
test_encodings (void)
{
	static const struct {
		const char *name;
		const char *mime_name;
		const char *bytes;
		gsize length;
	} cases[] = {
		{ "utf8", "utf-8", "caf\303\251", 5 },
		{ "latin1", "iso-8859-1", "caf\351", 4 },
		{ "CP1252", "windows-1252", "caf\351", 4 },
		{ "UTF-16", "utf-16", "c\0a\0f\0\351\0", 8 },
		{ "utf16", "utf-16", "c\0a\0f\0\351\0", 8 },
		{ "Unicode", "utf-16", "c\0a\0f\0\351\0", 8 },
		{ "csUnicode", "utf-16", "c\0a\0f\0\351\0", 8 },
		{ "utf-16le", "utf-16le", "c\0a\0f\0\351\0", 8 },
		{ "utf-16be", "utf-16be", "\0c\0a\0f\0\351", 8 },
		{ "UTF-32", "utf-32", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf32", "utf-32", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf-32le", "utf-32le", "c\0\0\0a\0\0\0f\0\0\0\351\0\0\0", 16 },
		{ "utf-32be", "utf-32be", "\0\0\0c\0\0\0a\0\0\0f\0\0\0\351", 16 },
	};
	GMimeBodyBuilder *builder = g_mime_body_builder_new ();
	GMimeEntity *body, *part;
	GError *error = NULL;
	guint i, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_assert_true (g_mime_body_builder_set_body_encoding (builder, cases[i].name, &error));
		g_assert_no_error (error);
		g_assert_cmpstr (g_mime_body_builder_get_body_encoding (builder), ==, cases[i].mime_name);
		g_assert_true (g_mime_body_builder_set_body_encoding (builder, g_mime_body_builder_get_body_encoding (builder), &error));
		g_assert_no_error (error);
		g_mime_body_builder_set_text_body (builder, NULL);
		g_mime_body_builder_set_html_body (builder, NULL);
		body = g_mime_body_builder_to_message_body (builder, &error);
		g_assert_no_error (error);
		check_text (body, "plain", cases[i].mime_name, "");
		check_bytes (body, (const guint8 *) "", 0);
		g_object_unref (body);
		g_mime_body_builder_set_text_body (builder, "caf\303\251");
		g_mime_body_builder_set_html_body (builder, "caf\303\251");
		body = g_mime_body_builder_to_message_body (builder, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MULTIPART_ALTERNATIVE (body));
		for (j = 0; j < 2; j++) {
			part = g_mime_multipart_get_part (GMIME_MULTIPART (body), j);
			check_text (part, j ? "html" : "plain", cases[i].mime_name, "caf\303\251");
			check_bytes (part, (const guint8 *) cases[i].bytes, cases[i].length);
		}
		g_object_unref (body);
	}
	g_object_unref (builder);
}

static void
test_failures (void)
{
	static const char *invalid[] = { "", "not-a-body-charset", "utf-8//IGNORE", "ascii//TRANSLIT", "utf-8 ", "\377" };
	GMimeBodyBuilder *builder = g_mime_body_builder_new ();
	GMimeEntity *attachment = (GMimeEntity *) g_mime_part_new ();
	GMimeEntity *body;
	GError *error = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		g_assert_false (g_mime_body_builder_set_body_encoding (builder, invalid[i], &error));
		if (i == 1) {
			g_assert_nonnull (error);
			g_assert_cmpuint (error->domain, ==, G_CONVERT_ERROR);
		} else {
			g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION);
		}
		g_clear_error (&error);
		g_assert_cmpstr (g_mime_body_builder_get_body_encoding (builder), ==, "utf-8");
	}
	g_assert_true (g_mime_body_builder_set_body_encoding (builder, "us-ascii", &error));
	g_assert_no_error (error);
	g_mime_attachment_collection_add (g_mime_body_builder_get_attachments (builder), attachment);
	g_mime_body_builder_set_text_body (builder, "plain");
	g_mime_body_builder_set_html_body (builder, "\342\202\254");
	g_assert_null (g_mime_body_builder_to_message_body (builder, &error));
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE);
	g_clear_error (&error);
	g_assert_cmpint (g_mime_attachment_collection_get_count (g_mime_body_builder_get_attachments (builder)), ==, 1);
	g_assert_cmpstr (g_mime_body_builder_get_text_body (builder), ==, "plain");
	g_assert_cmpstr (g_mime_body_builder_get_html_body (builder), ==, "\342\202\254");
	g_assert_cmpstr (g_mime_body_builder_get_body_encoding (builder), ==, "us-ascii");
	g_mime_body_builder_set_html_body (builder, NULL);
	g_mime_body_builder_set_text_body (builder, "\342\202\254");
	g_assert_null (g_mime_body_builder_to_message_body (builder, &error));
	g_assert_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE);
	g_clear_error (&error);
	g_assert_true (g_mime_body_builder_set_body_encoding (builder, "utf-8", &error));
	g_assert_no_error (error);
	body = g_mime_body_builder_to_message_body (builder, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART (body));
	g_object_unref (body);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*charset != NULL*");
	g_assert_false (g_mime_body_builder_set_body_encoding (builder, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_body_builder_set_text_body (builder, "\377");
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_mime_body_builder_set_html_body (builder, "\377");
	g_test_assert_expected_messages ();
	g_assert_cmpstr (g_mime_body_builder_get_text_body (builder), ==, "\342\202\254");
	g_assert_null (g_mime_body_builder_get_html_body (builder));
	g_assert_no_error (error);
	g_object_unref (attachment);
	g_object_unref (builder);
}

static void
mark_destroyed (gpointer data, GObject *object)
{
	gboolean *destroyed = data;

	(void) object;
	*destroyed = TRUE;
}

static void
test_ownership (void)
{
	GMimeBodyBuilder *builder = g_mime_body_builder_new ();
	GMimeAttachmentCollection *attachments, *resources;
	GMimeEntity *entity = (GMimeEntity *) g_mime_part_new ();
	GMimeEntity *body;
	GError *error = NULL;
	gboolean destroyed = FALSE;

	attachments = g_object_ref (g_mime_body_builder_get_attachments (builder));
	resources = g_object_ref (g_mime_body_builder_get_linked_resources (builder));
	g_object_weak_ref (G_OBJECT (entity), mark_destroyed, &destroyed);
	g_mime_attachment_collection_add (attachments, entity);
	g_mime_attachment_collection_add (resources, entity);
	g_object_unref (entity);
	body = g_mime_body_builder_to_message_body (builder, &error);
	g_assert_no_error (error);
	g_assert_true (body == entity);
	g_object_unref (builder);
	g_assert_cmpint (g_mime_attachment_collection_get_count (attachments), ==, 1);
	g_assert_cmpint (g_mime_attachment_collection_get_count (resources), ==, 1);
	g_mime_attachment_collection_clear (attachments);
	g_mime_attachment_collection_clear (resources);
	g_object_unref (attachments);
	g_object_unref (resources);
	g_assert_false (destroyed);
	g_assert_true (GMIME_IS_PART (body));
	g_object_unref (body);
	g_assert_true (destroyed);

	builder = g_mime_body_builder_new ();
	attachments = g_mime_body_builder_get_attachments (builder);
	resources = g_mime_body_builder_get_linked_resources (builder);
	entity = g_mime_attachment_collection_add_data (resources, "image.png", (const guint8 *) "data", 4, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, "inline");
	g_mime_attachment_collection_add (attachments, entity);
	g_mime_body_builder_set_text_body (builder, "plain");
	g_mime_body_builder_set_html_body (builder, "<img src=\"image.png\">");
	destroyed = FALSE;
	g_object_weak_ref (G_OBJECT (entity), mark_destroyed, &destroyed);
	g_object_unref (entity);
	body = g_mime_body_builder_to_message_body (builder, &error);
	g_assert_no_error (error);
	g_object_unref (builder);
	g_assert_false (destroyed);
	check_tree (body, "plain", "<img src=\"image.png\">", &entity, 1, &entity, 1);
	g_object_unref (body);
	g_assert_true (destroyed);
}

static void
test_roundtrip (void)
{
	static const guint8 bytes[] = { 0, 127, 128, 255 };
	GMimeBodyBuilder *builder = g_mime_body_builder_new ();
	GMimeEntity *body, *parsed, *part;
	GMimeMultipart *mixed, *alternative, *related;
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeParser *parser;
	GError *error = NULL;
	char *text;

	g_mime_body_builder_set_text_body (builder, "caf\303\251");
	g_mime_body_builder_set_html_body (builder, "<b>caf\303\251</b>");
	part = g_mime_attachment_collection_add_data (g_mime_body_builder_get_attachments (builder),
						    "file.bin", bytes, sizeof (bytes), NULL, &error);
	g_assert_no_error (error);
	g_object_unref (part);
	part = g_mime_attachment_collection_add_data (g_mime_body_builder_get_linked_resources (builder),
						    "image.png", bytes, sizeof (bytes), NULL, &error);
	g_assert_no_error (error);
	g_object_unref (part);
	body = g_mime_body_builder_to_message_body (builder, &error);
	g_assert_no_error (error);
	g_object_unref (builder);
	g_assert_true (g_mime_entity_prepare (body, GMIME_ENCODING_CONSTRAINT_7BIT, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_entity_write_to_stream (body, NULL, stream), >, 0);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	parser = g_mime_parser_new_with_stream (stream);
	parsed = g_mime_parser_construct_part (parser, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MULTIPART (parsed));
	mixed = GMIME_MULTIPART (parsed);
	g_assert_cmpint (g_mime_multipart_get_count (mixed), ==, 2);
	part = g_mime_multipart_get_part (mixed, 1);
	g_assert_true (GMIME_IS_PART (part));
	g_assert_cmpstr (g_mime_part_get_filename (GMIME_PART (part)), ==, "file.bin");
	check_bytes (part, bytes, sizeof (bytes));
	part = g_mime_multipart_get_part (mixed, 0);
	g_assert_true (GMIME_IS_MULTIPART_ALTERNATIVE (part));
	alternative = GMIME_MULTIPART (part);
	g_assert_cmpint (g_mime_multipart_get_count (alternative), ==, 2);
	part = g_mime_multipart_get_part (alternative, 0);
	g_assert_true (GMIME_IS_TEXT_PART (part));
	text = g_mime_text_part_get_text (GMIME_TEXT_PART (part));
	g_assert_cmpstr (text, ==, "caf\303\251");
	g_free (text);
	part = g_mime_multipart_get_part (alternative, 1);
	g_assert_true (GMIME_IS_MULTIPART_RELATED (part));
	related = GMIME_MULTIPART (part);
	g_assert_cmpint (g_mime_multipart_get_count (related), ==, 2);
	part = g_mime_multipart_related_get_root (GMIME_MULTIPART_RELATED (related));
	g_assert_true (part == g_mime_multipart_get_part (related, 0));
	g_assert_true (GMIME_IS_TEXT_PART (part));
	text = g_mime_text_part_get_text (GMIME_TEXT_PART (part));
	g_assert_cmpstr (text, ==, "<b>caf\303\251</b>");
	g_free (text);
	part = g_mime_multipart_get_part (related, 1);
	g_assert_cmpstr (g_mime_entity_get_disposition (part), ==, "inline");
	g_assert_cmpstr (g_mime_entity_get_content_location (part), ==, "image.png");
	check_bytes (part, bytes, sizeof (bytes));
	g_object_unref (parsed);
	g_object_unref (parser);
	g_object_unref (stream);
	g_object_unref (body);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MimeKit body builder");
	test_defaults_and_strings ();
	test_structures ();
	test_encodings ();
	test_failures ();
	test_ownership ();
	test_roundtrip ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
