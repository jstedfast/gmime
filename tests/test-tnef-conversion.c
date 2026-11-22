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
#include <stdlib.h>
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-tnef-conversion.h>
#include <gmime/gmime-tnef-part.h>

#define TAG(name) GMIME_TNEF_PROPERTY_TAG_##name
#define ATTR(name) GMIME_TNEF_ATTRIBUTE_TAG_##name

typedef struct {
	GMimeStream *stream;
	GMimeTnefWriter *writer;
} Fixture;

static void
start (Fixture *f)
{
	f->stream = g_mime_stream_mem_new ();
	f->writer = g_mime_tnef_writer_new (f->stream, 1252, 0, NULL);
	g_assert_nonnull (f->writer);
}

static void
string (GMimeTnefPropertyWriter *pw, GMimeTnefPropertyTag tag, const char *s)
{
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, tag, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, s, NULL));
}

static void
integer (GMimeTnefPropertyWriter *pw, GMimeTnefPropertyTag tag, int n)
{
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, tag, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_int32 (pw, n, NULL));
}

static void
binary (GMimeTnefPropertyWriter *pw, GMimeTnefPropertyTag tag, const char *s, gsize n)
{
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, tag, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_bytes (pw, (const guint8 *) s, n, NULL));
}

static void
end_props (GMimeTnefPropertyWriter *pw)
{
	g_assert_true (g_mime_tnef_property_writer_close (pw, NULL));
	g_object_unref (pw);
}

static GMimeTnefPropertyWriter *
attachment (Fixture *f, int method, const char *name)
{
	static const guint8 render[14] = { 1 };
	GMimeTnefPropertyWriter *pw;
	g_assert_true (g_mime_tnef_writer_write_attribute_bytes (f->writer, ATTR (ATTACH_RENDER_DATA), render, sizeof render, NULL));
	pw = g_mime_tnef_writer_open_property_writer (f->writer, ATTR (ATTACHMENT), NULL);
	integer (pw, TAG (ATTACH_METHOD), method);
	string (pw, TAG (ATTACH_LONG_FILENAME_W), name);
	return pw;
}

/* Deliberately destroy the input before inspecting the converted message. */
static GMimeTnefConversionResult *
finish (Fixture *f, GMimeTnefConversionOptions *opts)
{
	GError *error = NULL;
	GMimeTnefMessage *source;
	GMimeTnefConversionResult *result;
	g_assert_true (g_mime_tnef_writer_close (f->writer, &error));
	g_assert_no_error (error);
	g_object_unref (f->writer);
	g_mime_stream_reset (f->stream);
	source = g_mime_tnef_message_load (f->stream, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (source);
	g_object_unref (f->stream);
	result = g_mime_tnef_message_convert_to_mime (source, opts, &error);
	g_assert_no_error (error);
	g_assert_nonnull (result);
	g_object_unref (source);
	return result;
}

static void
assert_text (GMimeEntity *entity, const char *expected)
{
	char *s = g_mime_text_part_get_text (GMIME_TEXT_PART (entity));
	g_assert_cmpstr (s, ==, expected);
	g_free (s);
}

static void
assert_content (GMimeEntity *entity, const char *expected, gsize n)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeContent *content = g_mime_part_get_content (GMIME_PART (entity));
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	g_assert_cmpint (g_mime_content_write_to_stream (content, stream), ==, n);
	g_assert_cmpmem (bytes->data, bytes->len, expected, n);
	g_object_unref (stream);
}

static void
test_skeleton (gconstpointer data)
{
	gboolean valid = GPOINTER_TO_INT (data) == 1;
	gboolean missing = GPOINTER_TO_INT (data) == 2;
	static const char skel[] = "Subject: Original\r\nContent-Type: multipart/mixed; boundary=z\r\n\r\n"
		"--z\r\nContent-Type: text/plain\r\n\r\n\r\n--z\r\nContent-Type: application/x-file\r\n"
		"X-Exchange-MIME-Skeleton-Content-Id: <file@example.org>\r\n\r\n\r\n--z--\r\n";
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionResult *result;
	GMimeMessage *message;
	GMimeMultipart *mixed;
	GMimeEntity *file;
	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	string (pw, TAG (SUBJECT_W), "Properties");
	string (pw, TAG (BODY_W), "Caf\xc3\xa9");
	const char *skeleton = missing ? "Subject: Original\r\nContent-Type: text/plain\r\n\r\n" : skel;
	binary (pw, TAG (MIME_SKELETON), skeleton, strlen (skeleton));
	end_props (pw);
	pw = attachment (&f, 1, "example.bin");
	string (pw, TAG (ATTACH_CONTENT_ID_W), valid ? "file@example.org" : "other@example.org");
	string (pw, TAG (ATTACH_MIME_TAG_W), "multipart/mixed");
	binary (pw, TAG (ATTACH_DATA_BIN), "\1\0\3", 3);
	end_props (pw);
	result = finish (&f, NULL);
	message = g_object_ref (g_mime_tnef_conversion_result_get_message (result));
	if (valid) {
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
		g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Original");
	} else {
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 1);
		g_assert_cmpint (g_mime_tnef_conversion_result_get_loss (result, 0)->kind, ==, GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON);
		g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Properties");
		if (missing)
			g_assert_nonnull (strstr (g_mime_tnef_conversion_result_get_loss (result, 0)->description, "does not contain"));
	}
	g_mime_tnef_conversion_result_unref (result);
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (message));
	g_assert_cmpint (g_mime_multipart_get_count (mixed), ==, 2);
	assert_text (g_mime_multipart_get_part (mixed, 0), "Caf\xc3\xa9");
	file = g_mime_multipart_get_part (mixed, 1);
	g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (file), "application", valid ? "x-file" : "octet-stream"));
	g_assert_null (g_mime_entity_get_header (file, "X-Exchange-MIME-Skeleton-Content-Id"));
	assert_content (file, "\1\0\3", 3);
	g_object_unref (message);
}

static void
test_embedded_skeleton (gconstpointer data)
{
	int kind = GPOINTER_TO_INT (data); /* 0: ordinary attachment, 1: message, 2: corrupt message */
	gboolean embedded = kind == 1;
	static const char corrupt[] = "\x07\x03\x02\0\0\0\0\0\xc0\0\0\0\0\0\0\x46" "bad TNEF";
	static const char skel[] = "Subject: Skeleton\r\nContent-Type: multipart/mixed; boundary=z\r\n\r\n"
		"--z\r\nContent-Type: text/plain\r\n\r\n\r\n--z\r\nContent-Type: message/rfc822\r\n"
		"X-Exchange-MIME-Skeleton-Content-Id: <inner@example.org>\r\n\r\n\r\n--z--\r\n";
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionResult *result;
	GMimeMessage *message;
	GMimeMultipart *mixed;
	GMimeEntity *file;

	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	string (pw, TAG (SUBJECT_W), "Properties");
	string (pw, TAG (BODY_W), "Outer body");
	binary (pw, TAG (MIME_SKELETON), skel, sizeof skel - 1);
	end_props (pw);
	pw = attachment (&f, kind ? 5 : 1, "attached");
	string (pw, TAG (ATTACH_CONTENT_ID_W), "inner@example.org");
	if (embedded) {
		GMimeTnefWriter *child;
		GMimeTnefPropertyWriter *properties;
		g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, TAG (ATTACH_DATA_OBJ), NULL));
		child = g_mime_tnef_property_writer_open_embedded_message (pw, 1252, NULL);
		properties = g_mime_tnef_writer_open_property_writer (child, ATTR (MAPI_PROPERTIES), NULL);
		string (properties, TAG (SUBJECT_W), "Inside");
		string (properties, TAG (BODY_W), "Inner body");
		end_props (properties);
		g_assert_true (g_mime_tnef_writer_close (child, NULL));
		g_object_unref (child);
	} else if (kind == 2) {
		binary (pw, TAG (ATTACH_DATA_OBJ), corrupt, sizeof corrupt - 1);
	} else {
		binary (pw, TAG (ATTACH_DATA_BIN), "ordinary attachment", 19);
	}
	end_props (pw);

	/* A message/rfc822 skeleton requests conversion even with default options. */
	result = finish (&f, NULL);
	message = g_mime_tnef_conversion_result_get_message (result);
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, embedded ? "Skeleton" : "Properties");
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (message));
	g_assert_cmpint (g_mime_multipart_get_count (mixed), ==, 2);
	assert_text (g_mime_multipart_get_part (mixed, 0), "Outer body");
	file = g_mime_multipart_get_part (mixed, 1);
	g_assert_null (g_mime_entity_get_header (file, "X-Exchange-MIME-Skeleton-Content-Id"));
	if (embedded) {
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
		g_assert_true (GMIME_IS_MESSAGE_PART (file));
		message = g_mime_message_part_get_message (GMIME_MESSAGE_PART (file));
		g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Inside");
		assert_text (g_mime_message_get_mime_part (message), "Inner body");
	} else {
		const char *reason;
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 1);
		g_assert_cmpint (g_mime_tnef_conversion_result_get_loss (result, 0)->kind, ==, GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON);
		reason = g_mime_tnef_conversion_result_get_loss (result, 0)->description;
		if (kind == 2) {
			g_assert_nonnull (strstr (reason, "could not be converted"));
			g_assert_true (GMIME_IS_TNEF_PART (file));
			assert_content (file, corrupt + 16, sizeof corrupt - 17);
		} else {
			g_assert_nonnull (strstr (reason, "not an embedded message"));
			assert_content (file, "ordinary attachment", 19);
		}
	}
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_rtf_skeleton_loss (void)
{
	const char skel[] = "Subject: Skeleton\r\nContent-Type: text/rtf\r\n\r\n";
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionResult *result;
	GMimeMessage *message;
	guint invalid_rtf = 0, invalid_skeleton = 0;

	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	string (pw, TAG (SUBJECT_W), "Properties");
	string (pw, TAG (BODY_W), "Recovered body");
	binary (pw, TAG (RTF_COMPRESSED), "invalid RTF header", 18);
	binary (pw, TAG (MIME_SKELETON), skel, sizeof skel - 1);
	end_props (pw);
	result = finish (&f, NULL);
	message = g_mime_tnef_conversion_result_get_message (result);
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Properties");
	assert_text (g_mime_message_get_mime_part (message), "Recovered body");
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 2);
	for (guint i = 0; i < 2; i++) {
		const GMimeTnefConversionLoss *loss = g_mime_tnef_conversion_result_get_loss (result, i);
		invalid_rtf += loss->kind == GMIME_TNEF_CONVERSION_LOSS_INVALID_RTF_BODY;
		invalid_skeleton += loss->kind == GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON;
	}
	g_assert_cmpuint (invalid_rtf, ==, 1);
	g_assert_cmpuint (invalid_skeleton, ==, 1);
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_body_skeletons (void)
{
	static const struct {
		const char *skeleton, *subject;
		const char *reason;
	} cases[] = {
		{ "", "Properties", NULL },
		/* Without a Content-Type, the skeleton body is implicitly text/plain. */
		{ "Subject: Headers only\r\n", "Headers only", NULL },
		{ "Subject: No HTML body\r\nContent-Type: text/html\r\n\r\n", "Properties", "No promoted body" },
		{ "Not a header\r\n\r\nBody\r\n", "Properties", "could not be parsed" }
	};
	for (guint i = 0; i < G_N_ELEMENTS (cases); i++) {
		Fixture f;
		GMimeTnefPropertyWriter *pw;
		GMimeTnefConversionResult *result;
		GMimeMessage *message;

		start (&f);
		pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
		string (pw, TAG (SUBJECT_W), "Properties");
		string (pw, TAG (BODY_W), "Body from MAPI");
		binary (pw, TAG (MIME_SKELETON), cases[i].skeleton, strlen (cases[i].skeleton));
		end_props (pw);
		result = finish (&f, NULL);
		message = g_mime_tnef_conversion_result_get_message (result);
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, cases[i].reason ? 1 : 0);
		if (cases[i].reason) {
			const GMimeTnefConversionLoss *loss = g_mime_tnef_conversion_result_get_loss (result, 0);
			g_assert_cmpint (loss->kind, ==, GMIME_TNEF_CONVERSION_LOSS_INVALID_MIME_SKELETON);
			g_assert_nonnull (strstr (loss->description, cases[i].reason));
		}
		g_assert_cmpstr (g_mime_message_get_subject (message), ==, cases[i].subject);
		assert_text (g_mime_message_get_mime_part (message), "Body from MAPI");
		g_mime_tnef_conversion_result_unref (result);
	}
}

static void
test_nul_references (gconstpointer data)
{
	static const char refs[] = "<root@example.org> <bad\0id@example.org> <> <parent@example.org>";
	gboolean wide = data != NULL;
	guint32 length = sizeof refs * (wide ? 2 : 1);
	guint32 header[] = { GUINT32_TO_LE (1), GUINT32_TO_LE (wide ? TAG (INTERNET_REFERENCES_W) : TAG (INTERNET_REFERENCES_A)),
			     GUINT32_TO_LE (1), GUINT32_TO_LE (length) };
	GByteArray *wire = g_byte_array_new ();
	GMimeTnefConversionResult *result;
	GMimeMessageIdList *list;
	Fixture f;

	/* Write the MAPI attribute directly: the public string writer takes a C string. */
	g_byte_array_append (wire, (guint8 *) header, sizeof header);
	for (guint i = 0; i < sizeof refs; i++) {
		guint8 unit[] = { refs[i], 0 };
		g_byte_array_append (wire, unit, wide ? 2 : 1);
	}
	while (wire->len % 4)
		g_byte_array_append (wire, (const guint8 *) "", 1);
	start (&f);
	g_assert_true (g_mime_tnef_writer_write_attribute_bytes (f.writer, ATTR (MAPI_PROPERTIES), wire->data, wire->len, NULL));
	g_byte_array_unref (wire);
	result = finish (&f, NULL);
	list = g_mime_message_get_references (g_mime_tnef_conversion_result_get_message (result));
	g_assert_cmpint (g_mime_message_id_list_length (list), ==, 2);
	g_assert_cmpstr (g_mime_message_id_list_get_message_id (list, 0), ==, "root@example.org");
	g_assert_cmpstr (g_mime_message_id_list_get_message_id (list, 1), ==, "parent@example.org");
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 1);
	g_assert_cmpint (g_mime_tnef_conversion_result_get_loss (result, 0)->kind, ==, GMIME_TNEF_CONVERSION_LOSS_INVALID_MESSAGE_ID);
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_transport_header_order (void)
{
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionResult *result;
	GMimeMessage *message;
	GMimeHeaderList *headers;

	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	string (pw, TAG (SUBJECT_W), "Original");
	string (pw, TAG (TRANSPORT_MESSAGE_HEADERS_W),
		"Received: from last.example by inbox.example\r\n"
		"Subject: Transport subject\r\nX-Transport: dropped\r\n"
		"Received: from first.example\r\n\tby last.example\r\n");
	string (pw, TAG (IN_REPLY_TO_ID_W), "<parent@example.org>");
	string (pw, TAG (INTERNET_REFERENCES_W), "<root@example.org> <parent@example.org>");
	end_props (pw);
	result = finish (&f, NULL);
	message = g_mime_tnef_conversion_result_get_message (result);
	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
	g_assert_cmpint (g_mime_header_list_get_count (headers), >=, 3);
	for (int i = 0; i < g_mime_header_list_get_count (headers); i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (headers, i);
		if (i < 2) {
			g_assert_cmpint (g_mime_header_get_id (header), ==, GMIME_HEADER_RECEIVED);
			g_assert_true (g_str_has_prefix (g_mime_header_get_value (header), i ? "from first.example" : "from last.example"));
		} else {
			g_assert_cmpint (g_mime_header_get_id (header), !=, GMIME_HEADER_RECEIVED);
		}
	}
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Original");
	g_assert_null (g_mime_entity_get_header (GMIME_ENTITY (message), "X-Transport"));
	g_assert_cmpstr (g_mime_message_get_in_reply_to (message), ==, "parent@example.org");
	GMimeMessageIdList *refs = g_mime_message_get_references (message);
	g_assert_cmpint (g_mime_message_id_list_length (refs), ==, 2);
	g_assert_cmpstr (g_mime_message_id_list_get_message_id (refs, 0), ==, "root@example.org");
	g_assert_cmpstr (g_mime_message_id_list_get_message_id (refs, 1), ==, "parent@example.org");
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_inline_and_headers (void)
{
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefConversionResult *result;
	GMimeTnefNameId *header;
	GMimeMessage *message;
	GMimeMultipart *mixed, *related;
	GMimeEntity *image;
	const char *html = "<img src='cid:pi%63@example.org'><p>cid:other@example.org</p>";
	guint i;
	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	string (pw, TAG (TRANSPORT_MESSAGE_HEADERS_W), "Received: by relay\r\nSubject: Ignore me");
	string (pw, TAG (SUBJECT_PREFIX_W), "Re: ");
	string (pw, TAG (NORMALIZED_SUBJECT_W), "Example");
	string (pw, TAG (INTERNET_MESSAGE_ID_W), "<abc@example.org>");
	string (pw, TAG (SENDER_EMAIL_ADDRESS_W), "from@example.org");
	binary (pw, TAG (BODY_HTML_B), html, strlen (html));
	header = g_mime_tnef_name_id_new_with_name (GMIME_TNEF_PROPERTY_SET_INTERNET_HEADERS, "X-Example");
	g_assert_true (g_mime_tnef_property_writer_write_named_property (pw, header, GMIME_TNEF_PROPERTY_TYPE_UNICODE, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, "retained", NULL));
	g_mime_tnef_name_id_unref (header);
	end_props (pw);
	for (i = 0; i < 2; i++) {
		pw = attachment (&f, 1, i ? "other" : "pic");
		string (pw, TAG (ATTACH_CONTENT_ID_W), i ? "other@example.org" : "pic@example.org");
		integer (pw, TAG (ATTACH_FLAGS), 4);
		binary (pw, TAG (ATTACH_DATA_BIN), "payload", 7);
		end_props (pw);
	}
	result = finish (&f, NULL);
	message = g_mime_tnef_conversion_result_get_message (result);
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
	g_assert_cmpstr (g_mime_message_get_message_id (message), ==, "abc@example.org");
	g_assert_cmpstr (g_mime_message_get_subject (message), ==, "Re: Example");
	g_assert_cmpstr (g_mime_entity_get_header (GMIME_ENTITY (message), "X-Example"), ==, "retained");
	g_assert_nonnull (g_mime_entity_get_header (GMIME_ENTITY (message), "Received"));
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (message));
	g_assert_cmpint (g_mime_multipart_get_count (mixed), ==, 2);
	related = GMIME_MULTIPART (g_mime_multipart_get_part (mixed, 0));
	g_assert_true (GMIME_IS_MULTIPART_RELATED (related));
	image = g_mime_multipart_get_part (related, 1);
	g_assert_cmpstr (g_mime_entity_get_content_id (image), ==, "pic@example.org");
	g_assert_cmpstr (g_mime_entity_get_disposition (image), ==, "inline");
	g_assert_cmpstr (g_mime_entity_get_content_id (g_mime_multipart_get_part (mixed, 1)), ==, "other@example.org");
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_losses (void)
{
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefNameId *name;
	GMimeTnefConversionResult *result;
	GMimeMessage *message;
	GMimeMultipart *mixed;
	static const GMimeTnefConversionLossKind kinds[] = {
		GMIME_TNEF_CONVERSION_LOSS_INVALID_RTF_BODY, GMIME_TNEF_CONVERSION_LOSS_INVALID_MESSAGE_ID,
		GMIME_TNEF_CONVERSION_LOSS_INVALID_HEADER, GMIME_TNEF_CONVERSION_LOSS_UNSUPPORTED_ATTACH_METHOD,
		GMIME_TNEF_CONVERSION_LOSS_ATTACHMENT_WITHOUT_CONTENT, GMIME_TNEF_CONVERSION_LOSS_OLE_OBJECT_NOT_RENDERED
	};
	guint i;
	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	binary (pw, TAG (RTF_COMPRESSED), "invalid RTF header", 18);
	string (pw, TAG (INTERNET_MESSAGE_ID_W), "not an identifier");
	name = g_mime_tnef_name_id_new_with_name (GMIME_TNEF_PROPERTY_SET_INTERNET_HEADERS, "Invalid Field");
	g_assert_true (g_mime_tnef_property_writer_write_named_property (pw, name, GMIME_TNEF_PROPERTY_TYPE_UNICODE, NULL));
	g_assert_true (g_mime_tnef_property_writer_write_value_string (pw, "ignored", NULL));
	g_mime_tnef_name_id_unref (name);
	end_props (pw);
	end_props (attachment (&f, 3, "external"));
	end_props (attachment (&f, 1, "empty"));
	pw = attachment (&f, 6, "object");
	binary (pw, TAG (ATTACH_DATA_BIN), "original OLE", 12);
	end_props (pw);
	result = finish (&f, NULL);
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, G_N_ELEMENTS (kinds));
	for (i = 0; i < G_N_ELEMENTS (kinds); i++) {
		const GMimeTnefConversionLoss *loss = g_mime_tnef_conversion_result_get_loss (result, i);
		g_assert_cmpint (loss->kind, ==, kinds[i]);
		g_assert_nonnull (loss->description);
	}
	message = g_mime_tnef_conversion_result_get_message (result);
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (message));
	g_assert_cmpint (g_mime_multipart_get_count (mixed), ==, 1);
	assert_content (g_mime_multipart_get_part (mixed, 0), "original OLE", 12);
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_nested (gconstpointer convert)
{
	Fixture f;
	GMimeTnefPropertyWriter *pw;
	GMimeTnefWriter *nested;
	GMimeTnefConversionOptions *options = g_mime_tnef_conversion_options_new ();
	GMimeTnefConversionResult *result;
	GMimeEntity *entity;
	GMimeMessage *message;
	start (&f);
	pw = attachment (&f, 5, "attached");
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, TAG (ATTACH_DATA_OBJ), NULL));
	nested = g_mime_tnef_property_writer_open_embedded_message (pw, 1252, NULL);
	g_assert_nonnull (nested);
	g_assert_true (g_mime_tnef_writer_write_attribute_string (nested, ATTR (SUBJECT), "Inside", NULL));
	g_assert_true (g_mime_tnef_writer_close (nested, NULL));
	g_object_unref (nested);
	end_props (pw);
	g_mime_tnef_conversion_options_set_convert_embedded_messages (options, convert != NULL);
	result = finish (&f, options);
	g_mime_tnef_conversion_options_unref (options);
	message = g_mime_tnef_conversion_result_get_message (result);
	entity = g_mime_multipart_get_part (GMIME_MULTIPART (g_mime_message_get_mime_part (message)), 0);
	if (convert) {
		g_assert_true (GMIME_IS_MESSAGE_PART (entity));
		g_assert_cmpstr (g_mime_message_get_subject (g_mime_message_part_get_message (GMIME_MESSAGE_PART (entity))), ==, "Inside");
	} else {
		GMimeTnefMessage *source;
		g_assert_true (GMIME_IS_TNEF_PART (entity));
		source = g_mime_tnef_part_load_tnef_message (GMIME_TNEF_PART (entity), NULL, NULL);
		g_assert_nonnull (source);
		g_assert_cmpstr (g_mime_tnef_message_get_subject (source), ==, "Inside");
		g_object_unref (source);
	}
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
	g_mime_tnef_conversion_result_unref (result);
}

typedef struct {
	guint labels, renders, destroyed;
} Callbacks;

static char *
label_attachment (GMimeTnefAttachment *a, GMimeEntity *entity, gpointer data)
{
	Callbacks *calls = data;
	g_assert_nonnull (entity);
	calls->labels++;
	return g_strdup_printf ("<%s>", g_mime_tnef_attachment_get_file_name (a));
}

static GMimeStream *
ole_image (GMimeTnefAttachment *a, gpointer data)
{
	Callbacks *calls = data;
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("xxGIF89a", 8);
	g_assert_cmpint (g_mime_tnef_attachment_get_method (a), ==, GMIME_TNEF_ATTACH_METHOD_OLE);
	calls->renders++;
	g_mime_stream_seek (stream, 2, GMIME_STREAM_SEEK_SET);
	return stream;
}

static void
count_destroy (gpointer data)
{
	((Callbacks *) data)->destroyed++;
}

static void
test_rtf_callbacks (void)
{
	Fixture f;
	Callbacks calls = { 0 };
	GMimeTnefPropertyWriter *pw;
	GMimeStream *rtf;
	GMimeTnefConversionOptions *options, *copy;
	GMimeTnefConversionResult *result;
	GMimeMultipart *mixed, *related, *alt;
	GMimeEntity *image;
	char *text, *html, *id;
	start (&f);
	pw = g_mime_tnef_writer_open_property_writer (f.writer, ATTR (MAPI_PROPERTIES), NULL);
	g_assert_true (g_mime_tnef_property_writer_write_property_tag (pw, TAG (RTF_COMPRESSED), NULL));
	rtf = g_mime_tnef_property_writer_open_rtf_compressed_stream (pw, TRUE, NULL);
	g_assert_cmpint (g_mime_stream_write_string (rtf, "{\\rtf1\\ansi A\\objattph B\\objattph C}"), >, 0);
	g_assert_cmpint (g_mime_stream_close (rtf), ==, 0);
	g_object_unref (rtf);
	end_props (pw);
	pw = attachment (&f, 6, "object");
	binary (pw, TAG (ATTACH_DATA_BIN), "OLE", 3);
	integer (pw, TAG (RENDERING_POSITION), 20);
	end_props (pw);
	pw = attachment (&f, 1, "doc");
	binary (pw, TAG (ATTACH_DATA_BIN), "data", 4);
	integer (pw, TAG (RENDERING_POSITION), 1);
	end_props (pw);
	options = g_mime_tnef_conversion_options_new ();
	g_mime_tnef_conversion_options_set_attachment_placeholder_callback (options, label_attachment, &calls, count_destroy);
	g_mime_tnef_conversion_options_set_ole_object_converter (options, ole_image, &calls, count_destroy);
	copy = g_mime_tnef_conversion_options_clone (options);
	g_mime_tnef_conversion_options_unref (options);
	result = finish (&f, copy);
	g_mime_tnef_conversion_options_unref (copy);
	g_assert_cmpuint (calls.labels, ==, 2);
	g_assert_cmpuint (calls.renders, ==, 1);
	g_assert_cmpuint (calls.destroyed, ==, 2);
	g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result)));
	related = GMIME_MULTIPART (g_mime_multipart_get_part (mixed, 0));
	g_assert_true (GMIME_IS_MULTIPART_RELATED (related));
	alt = GMIME_MULTIPART (g_mime_multipart_get_part (related, 0));
	image = g_mime_multipart_get_part (related, 1);
	assert_content (image, "GIF89a", 6);
	g_assert_cmpstr (g_mime_part_get_filename (GMIME_PART (image)), ==, "object.gif");
	text = g_mime_text_part_get_text (GMIME_TEXT_PART (g_mime_multipart_get_part (alt, 0)));
	html = g_mime_text_part_get_text (GMIME_TEXT_PART (g_mime_multipart_get_part (alt, 1)));
	g_assert_nonnull (strstr (text, "<doc>"));
	g_assert_nonnull (strstr (text, "<object>"));
	g_assert_true (strstr (text, "<doc>") < strstr (text, "<object>"));
	g_assert_nonnull (strstr (html, "&lt;doc&gt;"));
	id = g_strconcat ("cid:", g_mime_entity_get_content_id (image), NULL);
	g_assert_nonnull (strstr (html, id));
	g_free (id);
	g_free (text);
	g_free (html);
	g_mime_tnef_conversion_result_unref (result);
}

static const char *corpus_directory;

typedef struct {
	const char *reference, *id, *base, *location;
	gboolean rendered, related;
} InlineCase;

static void
test_html_reference (gconstpointer user_data)
{
	const InlineCase *example = user_data;
	char *html = g_strdup_printf ("<html><body><img src=\"%s\"></body></html>", example->reference);
	GMimeTnefPropertyWriter *properties;
	GMimeTnefConversionResult *result;
	GMimeEntity *body, *part;
	Fixture fixture;

	start (&fixture);
	properties = g_mime_tnef_writer_open_property_writer (fixture.writer, ATTR (MAPI_PROPERTIES), NULL);
	binary (properties, TAG (BODY_HTML_B), html, strlen (html));
	end_props (properties);
	g_free (html);

	properties = attachment (&fixture, 1, "picture.gif");
	string (properties, TAG (ATTACH_MIME_TAG_W), "image/gif");
	if (example->id)
		string (properties, TAG (ATTACH_CONTENT_ID_W), example->id);
	if (example->base)
		string (properties, TAG (ATTACH_CONTENT_BASE_W), example->base);
	if (example->location)
		string (properties, TAG (ATTACH_CONTENT_LOCATION_W), example->location);
	integer (properties, TAG (ATTACH_FLAGS), example->rendered ? 4 : 0);
	binary (properties, TAG (ATTACH_DATA_BIN), "GIF89a", 6);
	end_props (properties);

	result = finish (&fixture, NULL);
	body = g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result));
	g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (body), "multipart",
						   example->related ? "related" : "mixed"));
	g_assert_cmpint (g_mime_multipart_get_count (GMIME_MULTIPART (body)), ==, 2);
	part = g_mime_multipart_get_part (GMIME_MULTIPART (body), 1);
	g_assert_cmpstr (g_mime_entity_get_disposition (part), ==, example->related ? "inline" : "attachment");
	assert_content (part, "GIF89a", 6);
	g_mime_tnef_conversion_result_unref (result);
}

static void
test_sender_headers (void)
{
	static const struct {
		const char *represented, *sender, *smtp, *from, *sender_header;
	} cases[] = {
		{ NULL, NULL, NULL, NULL, NULL },
		{ "boss@example.org", "assistant@example.org", NULL, "Boss <boss@example.org>", "Assistant <assistant@example.org>" },
		{ "alice@example.org", "ALICE@example.org", NULL, "Boss <alice@example.org>", NULL },
		{ NULL, "/O=EXAMPLE/CN=ASSISTANT", "assistant@example.org", "Assistant <assistant@example.org>", NULL }
	};

	for (guint index = 0; index < G_N_ELEMENTS (cases); index++) {
		Fixture fixture;
		GMimeTnefPropertyWriter *properties;
		GMimeTnefConversionResult *result;
		GMimeEntity *message;

		start (&fixture);
		properties = g_mime_tnef_writer_open_property_writer (fixture.writer, ATTR (MAPI_PROPERTIES), NULL);
		string (properties, TAG (BODY_W), "Hello");
		if (cases[index].represented) {
			string (properties, TAG (SENT_REPRESENTING_NAME_W), "Boss");
			string (properties, TAG (SENT_REPRESENTING_ADDRTYPE_W), "SMTP");
			string (properties, TAG (SENT_REPRESENTING_EMAIL_ADDRESS_W), cases[index].represented);
		}
		if (cases[index].sender) {
			string (properties, TAG (SENDER_NAME_W), "Assistant");
			string (properties, TAG (SENDER_ADDRTYPE_W), cases[index].smtp ? "EX" : "SMTP");
			string (properties, TAG (SENDER_EMAIL_ADDRESS_W), cases[index].sender);
		}
		if (cases[index].smtp)
			string (properties, TAG (SENDER_SMTP_ADDRESS_W), cases[index].smtp);
		end_props (properties);

		result = finish (&fixture, NULL);
		message = GMIME_ENTITY (g_mime_tnef_conversion_result_get_message (result));
		g_assert_cmpstr (g_mime_entity_get_header (message, "From"), ==, cases[index].from);
		g_assert_cmpstr (g_mime_entity_get_header (message, "Sender"), ==, cases[index].sender_header);
		g_assert_null (g_mime_entity_get_header (message, "Date"));
		g_assert_null (g_mime_entity_get_header (message, "Subject"));
		g_assert_null (g_mime_entity_get_header (message, "Message-Id"));
		g_mime_tnef_conversion_result_unref (result);
	}
}

static void
test_body_precedence (void)
{
	/* MS-OXBBODY 2.1.3.1: native format takes precedence; when that body
	 * is missing, choose among the available in-sync representations. */
	static const struct {
		int native;
		gboolean sync, plain, html, expect_rtf;
	} cases[] = {
		{ 0, FALSE, TRUE, FALSE, FALSE }, { 0, FALSE, TRUE, TRUE, FALSE },
		{ 0, TRUE, TRUE, TRUE, TRUE }, { 2, FALSE, TRUE, TRUE, TRUE },
		{ 1, FALSE, FALSE, FALSE, TRUE }, { 1, TRUE, TRUE, FALSE, FALSE },
		{ 3, TRUE, TRUE, TRUE, FALSE }
	};
	static const char html[] = "<p>HTML property</p>";

	for (guint index = 0; index < G_N_ELEMENTS (cases); index++) {
		Fixture fixture;
		GMimeTnefPropertyWriter *properties;
		GMimeStream *rtf;
		GMimeTnefConversionResult *result;
		GMimeEntity *body;

		start (&fixture);
		properties = g_mime_tnef_writer_open_property_writer (fixture.writer, ATTR (MAPI_PROPERTIES), NULL);
		if (cases[index].plain)
			string (properties, TAG (BODY_W), "Plain property");
		if (cases[index].html)
			binary (properties, TAG (BODY_HTML_B), html, sizeof html - 1);
		integer (properties, TAG (NATIVE_BODY), cases[index].native);
		g_assert_true (g_mime_tnef_property_writer_write_property_tag (properties, TAG (RTF_IN_SYNC), NULL));
		g_assert_true (g_mime_tnef_property_writer_write_value_boolean (properties, cases[index].sync, NULL));
		g_assert_true (g_mime_tnef_property_writer_write_property_tag (properties, TAG (RTF_COMPRESSED), NULL));
		rtf = g_mime_tnef_property_writer_open_rtf_compressed_stream (properties, index % 2, NULL);
		g_assert_cmpint (g_mime_stream_write_string (rtf, "{\\rtf1\\ansi RTF property}"), >, 0);
		g_assert_cmpint (g_mime_stream_close (rtf), ==, 0);
		g_object_unref (rtf);
		end_props (properties);

		result = finish (&fixture, NULL);
		g_assert_cmpuint (g_mime_tnef_conversion_result_get_loss_count (result), ==, 0);
		body = g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result));
		if (cases[index].html || cases[index].expect_rtf) {
			GMimeMultipart *alternative = GMIME_MULTIPART (body);
			g_assert_true (GMIME_IS_MULTIPART_ALTERNATIVE (alternative));
			g_assert_cmpuint (g_mime_multipart_get_count (alternative), ==, 2);
			for (int part = 0; part < 2; part++) {
				GMimeEntity *entity = g_mime_multipart_get_part (alternative, part);
				char *value = g_mime_text_part_get_text (GMIME_TEXT_PART (entity));
				g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (entity),
									    "text", part ? "html" : "plain"));
				if (cases[index].expect_rtf)
					g_assert_nonnull (strstr (value, "RTF property"));
				else
					g_assert_cmpstr (value, ==, part ? html : "Plain property");
				g_free (value);
			}
		} else {
			assert_text (body, "Plain property");
		}
		g_mime_tnef_conversion_result_unref (result);
	}
}

static void
test_attachment_media_types (void)
{
	static const char *const types[] = { "text/plain", "multipart/mixed; boundary=x", "message/rfc822",
		"message/partial", "application/applefile", "application/mac-binhex40", "not a mime type" };
	Fixture fixture;
	GMimeTnefConversionResult *result;
	GMimeMultipart *mixed;
	start (&fixture);
	for (guint index = 0; index < G_N_ELEMENTS (types); index++) {
		GMimeTnefPropertyWriter *properties = attachment (&fixture, 1, "file.bin");
		string (properties, TAG (ATTACH_MIME_TAG_W), types[index]);
		binary (properties, TAG (ATTACH_DATA_BIN), "ABC", 3);
		end_props (properties);
	}
	result = finish (&fixture, NULL);
	mixed = GMIME_MULTIPART (g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result)));
	g_assert_cmpuint (g_mime_multipart_get_count (mixed), ==, G_N_ELEMENTS (types));
	for (guint index = 0; index < G_N_ELEMENTS (types); index++) {
		GMimeEntity *entity = g_mime_multipart_get_part (mixed, index);
		char *type = g_mime_content_type_get_mime_type (g_mime_entity_get_content_type (entity));
		g_assert_cmpstr (type, ==, index ? "application/octet-stream" : "text/plain");
		g_assert_cmpstr (g_mime_part_get_filename (GMIME_PART (entity)), ==, "file.bin");
		assert_content (entity, "ABC", 3);
		g_free (type);
	}
	g_mime_tnef_conversion_result_unref (result);
}

static int
compare_property_lines (gconstpointer left, gconstpointer right)
{
	return strcmp (*(const char *const *) left, *(const char *const *) right);
}

/* MimeKit emits the properties of these two VEVENTs in lexical order.
 * Preserve component boundaries (including the nested reminder). Neither
 * fixture has folded event properties. */
static GString *
normalize_calendar (GByteArray *bytes)
{
	GString *calendar = g_string_new_len ((const char *) bytes->data, bytes->len);
	char *event, *end, *properties, **lines;
	gsize length;

	g_assert_cmpuint (g_string_replace (calendar, "\r\nPRODID:-//GMime//EN\r\n",
		"\r\nPRODID:-//.NET Foundation//MimeKit//EN\r\n", 0), ==, 1);
	event = strstr (calendar->str, "BEGIN:VEVENT\r\n");
	g_assert_nonnull (event);
	event += strlen ("BEGIN:VEVENT\r\n");
	end = strstr (event, "\r\nBEGIN:");
	if (!end)
		end = strstr (event, "\r\nEND:VEVENT");
	g_assert_nonnull (end);

	length = end - event;
	properties = g_strndup (event, length);
	lines = g_strsplit (properties, "\r\n", -1);
	qsort (lines, g_strv_length (lines), sizeof (char *), compare_property_lines);
	g_free (properties);
	properties = g_strjoinv ("\r\n", lines);
	memcpy (event, properties, length);
	g_free (properties);
	g_strfreev (lines);
	return calendar;
}

static void
append_snapshot_date (GString *description, const char *value)
{
	GDateTime *date = g_mime_utils_header_decode_date (value);
	g_assert_nonnull (date);
	GDateTime *utc = g_date_time_to_utc (date);
	char *formatted = g_date_time_format (utc, "%Y-%m-%dT%H:%M:%SZ");

	g_string_append (description, formatted);
	g_free (formatted);
	g_date_time_unref (utc);
	g_date_time_unref (date);
}

static void
describe_headers (GString *description, GMimeMessage *message)
{
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GPtrArray *lines = g_ptr_array_new_with_free_func (g_free);

	for (int index = 0; index < g_mime_header_list_get_count (headers); index++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (headers, index);
		const char *value = g_mime_header_get_value (header);
		char *field = g_ascii_strdown (g_mime_header_get_name (header), -1);
		/* GMime spells out these defaults; MimeKit leaves them implicit. */
		if ((!strcmp (field, "mime-version") && !g_strcmp0 (value, "1.0")) ||
		    ((!strcmp (field, "priority") || !strcmp (field, "importance")) && !g_strcmp0 (value, "normal"))) {
			g_free (field);
			continue;
		}
		GString *line = g_string_new (field);
		g_string_append_c (line, ':');
		if (!strcmp (field, "date")) {
			append_snapshot_date (line, value);
		} else if (value) {
			/* Ignore differences in header folding and horizontal whitespace. */
			gboolean space = FALSE;
			for (; *value; value++) {
				if (strchr (" \t\r\n", *value)) {
					space = TRUE;
				} else {
					if (space && line->len > strlen (field) + 1)
						g_string_append_c (line, ' ');
					g_string_append_c (line, *value);
					space = FALSE;
				}
			}
		}
		g_free (field);
		/* Avoid duplicating long transport diagnostics verbatim in fixtures. */
		if (line->len > 200) {
			char *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, line->str, line->len);
			char *separator = strchr (line->str, ':');
			g_string_truncate (line, separator - line->str + 1);
			g_string_append_printf (line, "sha256=%s", digest);
			g_free (digest);
		}
		g_ptr_array_add (lines, g_string_free (line, FALSE));
	}
	g_ptr_array_sort (lines, compare_property_lines);
	for (guint index = 0; index < lines->len; index++)
		g_string_append_printf (description, "%s\n", (char *) lines->pdata[index]);
	g_ptr_array_unref (lines);
}

static void
describe_entity (GString *description, GMimeEntity *entity, guint depth, const char *generated_id)
{
	static const char *const parameters[] = { "charset", "method", "name", "type" };
	char *type;
	guint indent;

	for (indent = 0; indent < depth; indent++)
		g_string_append (description, "  ");

	if (!entity) {
		g_string_append (description, "(no body)\n");
		return;
	}

	type = g_mime_content_type_get_mime_type (g_mime_entity_get_content_type (entity));
	g_string_append (description, type);
	g_free (type);
	/* The reference describes these four fields structurally below. Include
	 * any other part headers verbatim rather than silently dropping them. */
	GMimeHeaderList *headers = g_mime_entity_get_header_list (entity);
	gboolean seen_type = FALSE, seen_id = FALSE, seen_encoding = FALSE, seen_disposition = FALSE;
	for (int i = 0; i < g_mime_header_list_get_count (headers); i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (headers, i);
		gboolean *seen;
		switch (g_mime_header_get_id (header)) {
		case GMIME_HEADER_CONTENT_TYPE: seen = &seen_type; break;
		case GMIME_HEADER_CONTENT_DISPOSITION: seen = &seen_disposition; break;
		case GMIME_HEADER_CONTENT_TRANSFER_ENCODING: seen = &seen_encoding; break;
		case GMIME_HEADER_CONTENT_ID: seen = &seen_id; break;
		default:
			g_string_append_printf (description, "; %s=%s", g_mime_header_get_name (header), g_mime_header_get_value (header));
			continue;
		}
		g_assert_false (*seen);
		*seen = TRUE;
	}
	for (guint index = 0; index < G_N_ELEMENTS (parameters); index++) {
		const char *value = g_mime_entity_get_content_type_parameter (entity, parameters[index]);
		if (value)
			g_string_append_printf (description, "; %s=%s", parameters[index], value);
	}

	if (GMIME_IS_MULTIPART (entity)) {
		GMimeMultipart *multipart = GMIME_MULTIPART (entity);
		int index;

		g_string_append_c (description, '\n');
		for (index = 0; index < g_mime_multipart_get_count (multipart); index++)
			describe_entity (description, g_mime_multipart_get_part (multipart, index), depth + 1, generated_id);
	} else {
		GMimeContent *content = g_mime_part_get_content (GMIME_PART (entity));
		GMimeStream *memory = g_mime_stream_mem_new ();
		GMimeStream *output = g_mime_stream_filter_new (memory);
		GByteArray *bytes;
		GString *normalized = NULL;
		char *digest;
		const char *disposition = g_mime_entity_get_disposition (entity);
		const char *filename = g_mime_entity_get_content_disposition_parameter (entity, "filename");
		const char *size = g_mime_entity_get_content_disposition_parameter (entity, "size");
		const char *cid = g_mime_entity_get_content_id (entity);

		if (disposition)
			g_string_append_printf (description, "; disposition=%s", disposition);
		if (filename)
			g_string_append_printf (description, "; filename=%s", filename);
		if (size)
			g_string_append_printf (description, "; size=%s", size);
		static const char *const date_names[] = { "creation-date", "modification-date", "read-date" };
		for (guint index = 0; index < G_N_ELEMENTS (date_names); index++) {
			const char *value = g_mime_entity_get_content_disposition_parameter (entity, date_names[index]);
			if (value) {
				g_string_append_printf (description, "; %s=", date_names[index]);
				append_snapshot_date (description, value);
			}
		}
		if (cid)
			g_string_append_printf (description, "; cid=%s", g_strcmp0 (cid, generated_id) ? cid : "generated-0@snapshot");
		const char *encoding = g_mime_entity_get_header (entity, "Content-Transfer-Encoding");
		/* GMime explicitly chooses a transport encoding for generated text;
		 * MimeKit defers that choice until serialization. */
		if (encoding && (!GMIME_IS_TEXT_PART (entity) ||
		    g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", "calendar")))
			g_string_append_printf (description, "; encoding=%s", encoding);

		if (GMIME_IS_TEXT_PART (entity)) {
			GMimeFilter *filter = g_mime_filter_unix2dos_new (FALSE);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (output), filter);
			g_object_unref (filter);
		}

		g_assert_nonnull (content);
		g_assert_cmpint (g_mime_content_write_to_stream (content, output), >=, 0);
		g_assert_cmpint (g_mime_stream_flush (output), ==, 0);
		bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
		if (g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", "calendar")) {
			normalized = normalize_calendar (bytes);
		} else if (generated_id && g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), "text", "html")) {
			char *reference = g_strconcat ("cid:", generated_id, NULL);
			normalized = g_string_new_len ((const char *) bytes->data, bytes->len);
			g_assert_cmpuint (g_string_replace (normalized, reference, "cid:generated-0@snapshot", 0), ==, 1);
			g_free (reference);
		}
		if (normalized) {
			digest = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guint8 *) normalized->str, normalized->len);
			g_string_append_printf (description, " %" G_GSIZE_FORMAT " %s\n", normalized->len, digest);
			g_string_free (normalized, TRUE);
		} else {
			digest = g_compute_checksum_for_data (G_CHECKSUM_SHA256, bytes->data, bytes->len);
			g_string_append_printf (description, " %u %s\n", bytes->len, digest);
		}
		g_free (digest);
		g_object_unref (output);
		g_object_unref (memory);
	}
}

static void
test_corpus_conversion (gconstpointer user_data)
{
	const char *name = user_data;
	char *prefix = g_build_filename (corpus_directory, name, NULL);
	char *input = g_strconcat (prefix, ".tnef", NULL);
	char *snapshot = g_strconcat (prefix, ".tree", NULL);
	GString *actual = g_string_new (NULL);
	GError *error = NULL;
	GMimeStream *stream = g_mime_stream_file_open (input, "rb", &error);
	GMimeTnefConversionResult *result;
	GMimeTnefMessage *source;
	GMimeEntity *body;
	const char *generated_id = NULL;
	char *expected;

	g_assert_no_error (error);
	source = g_mime_tnef_message_load (stream, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (stream);
	result = g_mime_tnef_message_convert_to_mime (source, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (source);
	body = g_mime_message_get_mime_part (g_mime_tnef_conversion_result_get_message (result));
	describe_headers (actual, g_mime_tnef_conversion_result_get_message (result));
	if (!strcmp (name, "attachments") || !strcmp (name, "christmas")) {
		GMimeEntity *related = g_mime_multipart_get_part (GMIME_MULTIPART (body), 0);
		GMimeEntity *image = g_mime_multipart_get_part (GMIME_MULTIPART (related), 1);
		generated_id = g_mime_entity_get_content_id (image);
		g_assert_nonnull (generated_id);
	}
	describe_entity (actual, body, 0, generated_id);
	g_assert_true (g_file_get_contents (snapshot, &expected, NULL, &error));
	g_assert_cmpstr (actual->str, ==, expected);

	g_free (expected);
	g_mime_tnef_conversion_result_unref (result);
	g_string_free (actual, TRUE);
	g_free (snapshot);
	g_free (input);
	g_free (prefix);
}

int
main (int argc, char **argv)
{
	static const InlineCase references[] = {
		{ "CID:Image%31@Example.com", "<image1@example.com>", NULL, NULL, TRUE, TRUE },
		{ "cid:pic@host", "\xc2\xa0pic@host\xc2\xa0", NULL, NULL, TRUE, FALSE },
		{ "cid:%C2%A0pic@host%C2%A0", "pic@host", NULL, NULL, TRUE, FALSE },
		{ "cid:%C2%A0pic@host%C2%A0", "\xc2\xa0pic@host\xc2\xa0", NULL, NULL, TRUE, TRUE },
		{ "cid:pic@host", "pic@host", NULL, NULL, FALSE, FALSE },
		{ "cid:pic@host", NULL, NULL, NULL, TRUE, FALSE },
		{ "http://example.org/i/logo.gif", NULL, NULL, "http://example.org/i/logo.gif", TRUE, TRUE },
		{ "http://example.org/i/logo.gif", NULL, "http://example.org/i/", "logo.gif", TRUE, TRUE }
	};
	static const char *const corpus[] = {
		"attachments", "body", "christmas", "data-before-name", "garbage-at-end", "long-filename", "MAPI_ATTACH_DATA_OBJ",
		"MAPI_OBJECT", "missing-filenames", "multi-value-attribute", "one-file", "panic", "rtf",
		"multi-name-property", "triples", "two-files", "unicode-mapi-attr", "unicode-mapi-attr-name", "winmail"
	};
	guint index;
	int result;
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/tnef/conversion/sender-headers", test_sender_headers);
	g_test_add_func ("/tnef/conversion/transport-header-order", test_transport_header_order);
	g_test_add_func ("/tnef/conversion/body-skeletons", test_body_skeletons);
	g_test_add_data_func ("/tnef/conversion/embedded-skeleton", GINT_TO_POINTER (1), test_embedded_skeleton);
	g_test_add_data_func ("/tnef/conversion/embedded-skeleton-fallback", NULL, test_embedded_skeleton);
	g_test_add_data_func ("/tnef/conversion/embedded-skeleton-corrupt", GINT_TO_POINTER (2), test_embedded_skeleton);
	g_test_add_data_func ("/tnef/conversion/nul-references-unicode", GINT_TO_POINTER (1), test_nul_references);
	g_test_add_data_func ("/tnef/conversion/nul-references-string8", NULL, test_nul_references);
	g_test_add_func ("/tnef/conversion/rtf-skeleton-loss", test_rtf_skeleton_loss);
	g_test_add_func ("/tnef/conversion/body-precedence", test_body_precedence);
	g_test_add_func ("/tnef/conversion/attachment-media-types", test_attachment_media_types);
	for (index = 0; index < G_N_ELEMENTS (references); index++) {
		char *path = g_strdup_printf ("/tnef/conversion/html-reference/%u", index);
		g_test_add_data_func (path, references + index, test_html_reference);
		g_free (path);
	}
	corpus_directory = "data/tnef";
	for (int arg = 1; arg < argc; arg++) {
		if (argv[arg][0] != '-')
			corpus_directory = argv[arg];
	}
	for (index = 0; index < G_N_ELEMENTS (corpus); index++) {
		char *path = g_strconcat ("/tnef/conversion/corpus/", corpus[index], NULL);
		g_test_add_data_func (path, corpus[index], test_corpus_conversion);
		g_free (path);
	}
	g_test_add_data_func ("/tnef/conversion/skeleton", GINT_TO_POINTER (1), test_skeleton);
	g_test_add_data_func ("/tnef/conversion/fallback", NULL, test_skeleton);
	g_test_add_data_func ("/tnef/conversion/missing-skeleton-attachment", GINT_TO_POINTER (2), test_skeleton);
	g_test_add_data_func ("/tnef/conversion/embedded-converted", GINT_TO_POINTER (1), test_nested);
	g_test_add_data_func ("/tnef/conversion/embedded-preserved", NULL, test_nested);
	g_test_add_func ("/tnef/conversion/inline-and-headers", test_inline_and_headers);
	g_test_add_func ("/tnef/conversion/losses", test_losses);
	g_test_add_func ("/tnef/conversion/rtf-callbacks", test_rtf_callbacks);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
