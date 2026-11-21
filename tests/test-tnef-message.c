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
#include <string.h>
#include <gmime/gmime.h>
#include <gmime/gmime-tnef-message.h>

#define ATTR(name) GMIME_TNEF_ATTRIBUTE_TAG_##name
#define TAG(name) GMIME_TNEF_PROPERTY_TAG_##name

static void
append32 (GByteArray *buf, guint32 value)
{
	value = GUINT32_TO_LE (value);
	g_byte_array_append (buf, (guint8 *) &value, 4);
}

static GByteArray *
header (void)
{
	const guint8 data[] = { 0x78, 0x9f, 0x3e, 0x22, 0xab, 0xcd };
	return g_byte_array_append (g_byte_array_new (), data, sizeof data);
}

static void
attribute (GByteArray *wire, guint8 level, GMimeTnefAttributeTag tag, const void *data, guint len)
{
	guint16 sum = 0;
	guint i;
	for (i = 0; i < len; i++)
		sum += ((const guint8 *) data)[i];
	sum = GUINT16_TO_LE (sum);
	g_byte_array_append (wire, &level, 1);
	append32 (wire, tag);
	append32 (wire, len);
	g_byte_array_append (wire, data, len);
	g_byte_array_append (wire, (guint8 *) &sum, 2);
}

static void
variable (GByteArray *props, GMimeTnefPropertyTag tag, const void *data, guint len)
{
	const guint8 zero[4] = { 0 };
	append32 (props, tag);
	append32 (props, 1);
	append32 (props, len);
	g_byte_array_append (props, data, len);
	g_byte_array_append (props, zero, (4 - (len & 3)) & 3);
}

static GMimeStream *
input (GByteArray *buf)
{
	return g_mime_stream_mem_new_with_buffer ((const char *) buf->data, buf->len);
}

static void
test_preference (void)
{
	static const guint8 render[14] = { 0 };
	static const guint8 rtf[] = { 19, 0, 0, 0, 7, 0, 0, 0, 'M', 'E', 'L', 'A', 0, 0, 0, 0,
				      '{', '\\', 'r', 't', 'f', '1', '}' };
	GByteArray *buf = header (), *props = g_byte_array_new ();
	GMimeTnefMessage *message;
	GMimeTnefMessageBody *body;
	GMimeStream *stream, *a, *b;
	GMimeTnefRecipient *recipient;
	GMimeTnefAttachment *attachment;
	GError *error = NULL;
	char *text, block[8];

	attribute (buf, 1, ATTR (SUBJECT), "legacy", 7);
	attribute (buf, 1, ATTR (BODY), "legacy", 7);
	append32 (props, 1); /* table rows */
	append32 (props, 2);
	append32 (props, TAG (RECIPIENT_TYPE));
	append32 (props, 0x10000003);
	variable (props, TAG (SMTP_ADDRESS_A), "test@example.org", 17);
	attribute (buf, 1, ATTR (RECIPIENT_TABLE), props->data, props->len);
	g_byte_array_set_size (props, 0);
	append32 (props, 6);
	variable (props, TAG (SUBJECT_A), "preferred", 10);
	variable (props, TAG (BODY_A), "caf\xc3\xa9\0", 7);
	variable (props, TAG (BODY_W), "x\0\0", 4);
	variable (props, TAG (BODY_HTML_B), "<b>\xc3\xa9", 5);
	variable (props, TAG (RTF_COMPRESSED), rtf, sizeof rtf);
	append32 (props, TAG (INTERNET_CODEPAGE));
	append32 (props, 65001);
	attribute (buf, 1, ATTR (MAPI_PROPERTIES), props->data, props->len);
	attribute (buf, 2, ATTR (ATTACH_RENDER_DATA), render, sizeof render);
	attribute (buf, 2, ATTR (ATTACH_TITLE), "a.bin", 6);
	attribute (buf, 2, ATTR (ATTACH_DATA), "legacy", 6);
	g_byte_array_set_size (props, 0);
	append32 (props, 2);
	variable (props, TAG (ATTACH_DATA_BIN), "data", 4);
	variable (props, TAG (ATTACH_DATA_BIN), "ignored", 7);
	attribute (buf, 2, ATTR (ATTACHMENT), props->data, props->len);
	stream = input (buf);
	message = g_mime_tnef_message_load (stream, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (stream);
	g_byte_array_unref (buf);
	g_byte_array_unref (props);
	g_assert_cmpstr (g_mime_tnef_message_get_subject (message), ==, "preferred");
	g_assert_cmpint (g_mime_tnef_message_get_codepage (message), ==, 65001);
	g_assert_cmpuint (g_mime_tnef_message_get_legacy_key (message), ==, 0xcdab);
	g_assert_cmpuint (g_mime_tnef_message_get_recipient_count (message), ==, 1);
	recipient = g_mime_tnef_message_get_recipient (message, 0);
	g_assert_cmpint (g_mime_tnef_recipient_get_recipient_type (recipient), ==, GMIME_TNEF_RECIPIENT_TYPE_BCC);
	g_assert_cmpstr (g_mime_tnef_recipient_get_email_address (recipient), ==, "test@example.org");
	body = g_mime_tnef_message_get_text_body (message);
	g_assert_cmpint (g_mime_tnef_message_body_get_length (body), ==, 5);
	text = g_mime_tnef_message_body_get_text (body, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (text, ==, "caf\xc3\xa9");
	g_free (text);
	text = g_mime_tnef_message_body_get_text (g_mime_tnef_message_get_html_body (message), &error);
	g_assert_no_error (error);
	g_assert_cmpstr (text, ==, "<b>\xc3\xa9");
	g_free (text);
	body = g_mime_tnef_message_get_rtf_body (message);
	g_assert_null (g_mime_tnef_message_body_get_charset (body));
	text = g_mime_tnef_message_body_get_text (body, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (text, ==, "{\\rtf1}");
	g_free (text);
	g_assert_cmpuint (g_mime_tnef_message_get_attachment_count (message), ==, 1);
	attachment = g_mime_tnef_message_get_attachment (message, 0);
	g_assert_cmpstr (g_mime_tnef_attachment_get_file_name (attachment), ==, "a.bin");
	a = g_mime_tnef_attachment_open_read (attachment);
	b = g_mime_tnef_attachment_open_read (attachment);
	g_object_unref (message);
	g_assert_cmpint (g_mime_stream_read (a, block, 2), ==, 2);
	g_assert_cmpint (g_mime_stream_read (b, block, sizeof block), ==, 4);
	g_assert_cmpmem (block, 4, "data", 4);
	g_assert_cmpint (g_mime_stream_read (a, block, sizeof block), ==, 2);
	g_assert_cmpmem (block, 2, "ta", 2);
	g_object_unref (a);
	g_object_unref (b);
}

static void
test_legacy_owner (void)
{
	static const guint8 owner[] = { 4, 0, 'B', 'o', 'b', 0, 8, 0, 'E', 'X', ':', 'u', 's', 'e', 'r', 0 };
	static const char message_class[] = " Microsoft Mail v3.0 IPM.Microsoft Schedule.MtgRespP ";
	GByteArray *buf = header ();
	GMimeTnefPropertySet *set;
	GMimeTnefProperty *p;
	GMimeTnefMessage *message;
	GMimeStream *stream;
	GError *error = NULL;
	attribute (buf, 1, ATTR (OWNER), owner, sizeof owner);
	attribute (buf, 1, ATTR (MESSAGE_CLASS), message_class, sizeof message_class);
	stream = input (buf);
	message = g_mime_tnef_message_load (stream, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (g_mime_tnef_message_get_message_class (message), ==, "IPM.Schedule.Meeting.Resp.Pos");
	set = g_mime_tnef_message_get_properties (message);
	p = g_mime_tnef_property_set_lookup (set, TAG (RCVD_REPRESENTING_EMAIL_ADDRESS_W));
	g_assert_nonnull (p);
	g_assert_cmpstr (g_value_get_string (g_mime_tnef_property_get_value (p, 0)), ==, "user");
	g_assert_null (g_mime_tnef_property_set_lookup (set, TAG (SENT_REPRESENTING_ENTRY_ID)));
	p = g_mime_tnef_property_set_lookup (set, TAG (RCVD_REPRESENTING_ENTRY_ID));
	g_assert_nonnull (p);
	g_assert_cmpuint (g_bytes_get_size (g_value_get_boxed (g_mime_tnef_property_get_value (p, 0))), ==,
			  24 + 2 * (4 + 3 + 5));
	g_object_unref (message);
	g_object_unref (stream);
	g_byte_array_unref (buf);
}

typedef struct {
	guint data_limit, depth_limit, destroyed;
	gint64 offset;
} Report;

static void
report_issue (const GMimeTnefComplianceIssue *issue, gpointer user_data)
{
	Report *report = user_data;
	if (issue->violation == GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED)
		report->data_limit++;
	else if (issue->violation == GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP)
		report->depth_limit++;
	else
		return;
	g_assert_cmpuint (issue->depth, ==, 1);
	report->offset = issue->stream_offset;
}

static void
report_destroy (gpointer user_data)
{
	((Report *) user_data)->destroyed++;
}

static void
test_replay (gconstpointer depth_case)
{
	GByteArray *inner = header (), *outer = header (), *props = g_byte_array_new ();
	const guint8 prefix[16] = { 0 };
	GByteArray *data = g_byte_array_append (g_byte_array_new (), prefix, sizeof prefix);
	GMimeTnefOptions *options = g_mime_tnef_options_new ();
	GMimeTnefReader *reader;
	GMimeTnefMessage *message, *child;
	GMimeTnefAttachment *attachment;
	GMimeStream *stream, *weak_stream;
	GError *error = NULL;
	Report report = { 0 };
	attribute (inner, 1, ATTR (SUBJECT), "nested", 7);
	g_byte_array_append (data, inner->data, inner->len);
	append32 (props, 2);
	variable (props, TAG (ATTACH_DATA_BIN), data->data, data->len);
	append32 (props, TAG (ATTACH_METHOD));
	append32 (props, GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE);
	attribute (outer, 2, ATTR (ATTACHMENT), props->data, props->len);
	/* Only the first replay may materialize the subject. */
	g_mime_tnef_options_set_max_total_data_bytes (options, data->len + 7);
	if (depth_case)
		g_mime_tnef_options_set_max_nesting_depth (options, 0);
	weak_stream = stream = input (outer);
	g_object_add_weak_pointer (G_OBJECT (stream), (gpointer *) &weak_stream);
	reader = g_mime_tnef_reader_new (stream, options);
	g_mime_tnef_reader_set_compliance_logger (reader, report_issue, &report, report_destroy);
	message = g_mime_tnef_message_load_from_reader (reader, &error);
	g_assert_no_error (error);
	attachment = g_object_ref (g_mime_tnef_message_get_attachment (message, 0));
	g_assert_true (g_mime_tnef_attachment_is_embedded_message (attachment));
	g_assert_cmpint (g_mime_tnef_attachment_get_length (attachment), ==, inner->len);
	g_object_unref (reader);
	g_object_unref (message);
	g_object_unref (stream);
	g_assert_null (weak_stream);
	g_assert_cmpuint (report.destroyed, ==, 0);
	g_assert_cmpuint (report.depth_limit, ==, 0);
	child = g_mime_tnef_attachment_load_embedded_message (attachment, &error);
	g_assert_no_error (error);
	if (depth_case) {
		g_assert_null (g_mime_tnef_message_get_subject (child));
		g_assert_cmpuint (report.depth_limit, ==, 1);
	} else {
		g_assert_cmpstr (g_mime_tnef_message_get_subject (child), ==, "nested");
		g_object_unref (child);
		child = g_mime_tnef_attachment_load_embedded_message (attachment, &error);
		g_assert_no_error (error);
		g_assert_null (g_mime_tnef_message_get_subject (child));
		g_assert_cmpuint (report.data_limit, ==, 1);
	}
	g_assert_cmpint (report.offset, >, 16);
	g_object_unref (child);
	g_object_unref (attachment);
	g_assert_cmpuint (report.destroyed, ==, 1);
	g_mime_tnef_options_unref (options);
	g_byte_array_unref (inner);
	g_byte_array_unref (outer);
	g_byte_array_unref (data);
	g_byte_array_unref (props);
}

static void
test_limits (void)
{
	const guint8 render[14] = { 0 };
	GMimeTnefOptions *options = g_mime_tnef_options_new ();
	GByteArray *buf = header (), *props = g_byte_array_new ();
	GMimeTnefReader *reader;
	GMimeTnefMessage *message;
	GMimeTnefAttachment *attachment;
	GMimeStream *stream;
	GError *error = NULL;
	attribute (buf, 2, ATTR (ATTACH_RENDER_DATA), render, sizeof render);
	attribute (buf, 2, ATTR (ATTACH_DATA), "old", 3);
	append32 (props, 1);
	variable (props, TAG (ATTACH_DATA_BIN), "new but oversized", 17);
	attribute (buf, 2, ATTR (ATTACHMENT), props->data, props->len);
	attribute (buf, 2, ATTR (ATTACH_RENDER_DATA), render, sizeof render);
	g_mime_tnef_options_set_max_property_value_length (options, sizeof render);
	g_mime_tnef_options_set_max_attachments (options, 1);
	stream = input (buf);
	reader = g_mime_tnef_reader_new (stream, options);
	message = g_mime_tnef_message_load_from_reader (reader, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (g_mime_tnef_message_get_attachment_count (message), ==, 1);
	attachment = g_mime_tnef_message_get_attachment (message, 0);
	g_assert_true (g_mime_tnef_attachment_has_content (attachment));
	g_assert_cmpint (g_mime_tnef_attachment_get_length (attachment), ==, 3);
	g_object_unref (message);
	g_assert_null (g_mime_tnef_message_load_from_reader (reader, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	g_object_unref (reader);
	g_object_unref (stream);
	buf->data[0] = 0;
	stream = input (buf);
	g_assert_null (g_mime_tnef_message_load (stream, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_object_unref (stream);
	g_byte_array_unref (props);
	g_byte_array_unref (buf);
	g_mime_tnef_options_unref (options);
}

int
main (int argc, char **argv)
{
	int result;
	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/tnef-message/preference-and-storage", test_preference);
	g_test_add_func ("/tnef-message/legacy-owner", test_legacy_owner);
	g_test_add_data_func ("/tnef-message/replay-data-budget", NULL, test_replay);
	g_test_add_data_func ("/tnef-message/replay-depth-budget", GINT_TO_POINTER (1), test_replay);
	g_test_add_func ("/tnef-message/limits-and-errors", test_limits);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
