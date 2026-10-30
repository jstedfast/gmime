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

#include "testsuite.h"

static const char *padding[] = {
	"\302\240", "\342\200\210", "\343\200\200", "\v", "\f"
};

static void
test_header_whitespace (void)
{
	GMimePart *part = g_mime_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeHeader *header;
	GBytes *bytes;
	GError *error = NULL;
	char *value, *text, *encoded, *raw;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (padding); i++) {
		testsuite_check ("Header and part properties preserve non-linear whitespace [%u]", i);
		value = g_strconcat (padding[i], "text", padding[i], NULL);
		text = g_strconcat (" \t", value, "\r\n", NULL);
		raw = g_mime_header_unfold (text);
		g_assert_cmpstr (raw, ==, value);
		g_free (raw);
		g_free (text);
		header = g_mime_header_new ("Subject", value, "utf-8");
		g_assert_cmpstr (g_mime_header_get_value (header), ==, value);
		encoded = g_base64_encode ((const guchar *) value, strlen (value));
		raw = g_strdup_printf (" =?utf-8?b?%s?=\r\n", encoded);
		bytes = g_bytes_new (raw, strlen (raw));
		g_assert_true (g_mime_header_set_raw_value (header, bytes, &error));
		g_assert_no_error (error);
		g_assert_cmpstr (g_mime_header_get_value (header), ==, value);
		text = g_mime_header_get_value_with_charset (header, "utf-8", &error);
		g_assert_no_error (error);
		g_assert_cmpstr (text, ==, value);
		g_free (text);
		g_bytes_unref (bytes);
		g_free (raw);
		g_free (encoded);
		g_object_unref (header);

		g_mime_part_set_content_description (part, value);
		g_mime_part_set_content_md5 (part, value);
		g_assert_cmpstr (g_mime_part_get_content_description (part), ==, value);
		g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, value);
		g_mime_entity_set_header (entity, "Content-Description", value, NULL);
		g_mime_entity_set_header (entity, "Content-Md5", value, NULL);
		g_assert_cmpstr (g_mime_part_get_content_description (part), ==, value);
		g_assert_cmpstr (g_mime_part_get_content_md5 (part), ==, value);
		g_free (value);
		testsuite_check_passed ();
	}
	g_mime_entity_set_header (entity, "Content-Base", "\302\240https://example.com/\302\240", NULL);
	g_assert_null (g_mime_entity_get_content_base (entity));
	g_object_unref (part);
}

static void
test_token_whitespace (void)
{
	GMimeTextPart *part = g_mime_text_part_new ();
	GMimeEntity *entity = GMIME_ENTITY (part);
	GMimeMessagePartial *partial = g_mime_message_partial_new ("id", 1, 2);
	GMimeMultipartReport *report = g_mime_multipart_report_new ("delivery-status");
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeContentDisposition *disposition;
	char *value;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (padding); i++) {
		testsuite_check ("MIME tokens and numbers reject non-linear padding [%u]", i);
		value = g_strconcat (padding[i], "base64", NULL);
		g_assert_cmpint (g_mime_content_encoding_from_string (value), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_free (value);
		value = g_strconcat ("base64", padding[i], "junk", NULL);
		g_assert_cmpint (g_mime_content_encoding_from_string (value), ==, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_free (value);
		value = g_strconcat (padding[i], "flowed", padding[i], NULL);
		g_mime_entity_set_content_type_parameter (entity, "format", value);
		g_assert_false (g_mime_text_part_is_flowed (part));
		g_assert_cmpint (g_mime_text_part_get_format (part), ==, GMIME_TEXT_FORMAT_PLAIN);
		g_free (value);
		value = g_strconcat (padding[i], "inline", padding[i], NULL);
		disposition = g_mime_content_disposition_parse (NULL, value);
		g_assert_cmpstr (g_mime_content_disposition_get_disposition (disposition), ==, value);
		g_object_unref (disposition);
		g_free (value);
		value = g_strconcat (padding[i], "delivery-status", padding[i], NULL);
		g_mime_multipart_report_set_report_type (report, value);
		g_assert_cmpstr (g_mime_multipart_report_get_report_type (report), ==, value);
		g_free (value);
		value = g_strconcat (padding[i], "high", padding[i], NULL);
		g_mime_entity_set_header (GMIME_ENTITY (message), "Importance", value, NULL);
		g_assert_cmpint (g_mime_message_get_importance (message), ==, GMIME_MESSAGE_IMPORTANCE_NORMAL);
		g_free (value);
		value = g_strconcat (padding[i], "urgent", padding[i], NULL);
		g_mime_entity_set_header (GMIME_ENTITY (message), "Priority", value, NULL);
		g_assert_cmpint (g_mime_message_get_priority (message), ==, GMIME_MESSAGE_PRIORITY_NORMAL);
		g_free (value);
		value = g_strconcat (padding[i], "5", padding[i], NULL);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (partial), "number", value);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (partial), "total", value);
		g_mime_entity_set_header (entity, "Content-Duration", value, NULL);
		g_assert_cmpint (g_mime_message_partial_get_number (partial), ==, -1);
		g_assert_cmpint (g_mime_message_partial_get_total (partial), ==, -1);
		g_assert_cmpint (g_mime_part_get_content_duration (GMIME_PART (part)), ==, -1);
		g_free (value);
		testsuite_check_passed ();
	}
	testsuite_check ("MIME tokens and numbers accept ASCII linear padding");
	g_assert_cmpint (g_mime_content_encoding_from_string (" \tbase64\r\n"), ==, GMIME_CONTENT_ENCODING_BASE64);
	g_mime_entity_set_content_type_parameter (entity, "format", " \tflowed\r\n ");
	g_assert_true (g_mime_text_part_is_flowed (part));
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (partial), "number", " \t1\r\n ");
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (partial), "total", " \t2\r\n ");
	g_mime_entity_set_header (entity, "Content-Duration", " \t5\r\n ", NULL);
	g_assert_cmpint (g_mime_message_partial_get_number (partial), ==, 1);
	g_assert_cmpint (g_mime_message_partial_get_total (partial), ==, 2);
	g_assert_cmpint (g_mime_part_get_content_duration (GMIME_PART (part)), ==, 5);
	testsuite_check_passed ();
	g_object_unref (message);
	g_object_unref (report);
	g_object_unref (partial);
	g_object_unref (part);
}

static void
test_address_whitespace (void)
{
	GMimeInternetAddress *address, *parsed;
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GError *error = NULL;
	char *name, *value, *encoded, *text, *a, *b, *c;
	guint i, group;

	for (i = 0; i < G_N_ELEMENTS (padding); i++) {
		testsuite_check ("Decoded address names preserve non-linear whitespace [%u]", i);
		name = g_strconcat (padding[i], "Name", padding[i], NULL);
		address = g_mime_mailbox_address_new (name, "a@example.com", &error);
		g_assert_no_error (error);
		g_assert_cmpstr (g_mime_internet_address_get_name (address), ==, name);
		value = g_strconcat (" \t", name, " \t", NULL);
		g_mime_internet_address_set_name (address, value);
		g_assert_cmpstr (g_mime_internet_address_get_name (address), ==, name);
		g_free (value);
		g_object_unref (address);
		encoded = g_base64_encode ((const guchar *) name, strlen (name));
		text = g_strdup_printf ("a@example.com (=?utf-8?b?%s?=)", encoded);
		parsed = g_mime_mailbox_address_parse (NULL, text, -1, &error);
		g_assert_no_error (error);
		g_assert_nonnull (parsed);
		g_assert_cmpstr (g_mime_internet_address_get_name (parsed), ==, name);
		g_object_unref (parsed);
		g_free (text);
		g_free (encoded);
		if (i < 3) {
			for (group = 0; group < 2; group++) {
				text = group ? g_strdup_printf ("a@example.com (%s)", name) :
					g_strdup_printf ("\"%s\" <a@example.com>", name);
				parsed = g_mime_mailbox_address_parse (NULL, text, -1, &error);
				g_assert_no_error (error);
				g_assert_nonnull (parsed);
				g_assert_cmpstr (g_mime_internet_address_get_name (parsed), ==, name);
				g_object_unref (parsed);
				g_free (text);
			}
			value = g_strconcat (padding[i], "id@example.com", padding[i], NULL);
			text = g_strdup_printf ("<%s>", value);
			g_mime_entity_set_header (GMIME_ENTITY (message), "Message-Id", text, NULL);
			g_assert_cmpstr (g_mime_message_get_message_id (message), ==, value);
			g_mime_message_set_message_id (message, g_mime_message_get_message_id (message));
			g_assert_cmpstr (g_mime_message_get_message_id (message), ==, value);
			g_free (text);
			g_free (value);
		}
		g_free (name);
		testsuite_check_passed ();
	}

	a = g_strnfill (30, 'A');
	b = g_strnfill (30, 'B');
	c = g_strnfill (30, 'C');
	name = g_strconcat (a, "\302\240", b, "\342\200\210", c, NULL);
	g_free (a);
	g_free (b);
	g_free (c);
	g_mime_format_options_set_allow_international (options, TRUE);
	for (group = 0; group < 2; group++) {
		testsuite_check ("International display-name folding preserves Unicode whitespace [%u]", group);
		address = group ? g_mime_group_address_new (name) :
			g_mime_mailbox_address_new (name, "a@example.com", &error);
		g_assert_no_error (error);
		text = g_mime_internet_address_to_string (address, options, TRUE);
		parsed = g_mime_internet_address_parse (NULL, text, -1, &error);
		g_assert_no_error (error);
		g_assert_nonnull (parsed);
		g_assert_cmpstr (g_mime_internet_address_get_name (parsed), ==, name);
		g_free (text);
		g_object_unref (parsed);
		g_object_unref (address);
		testsuite_check_passed ();
	}
	g_free (name);
	g_mime_format_options_free (options);
	g_object_unref (message);
}

static void
test_parameter_whitespace (void)
{
	const char *filename = " \302\240report.txt\343\200\200 ";
	GMimeMultipart *multipart;
	GMimePart *part;
	GMimeEntity *parsed, *child;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *error = NULL;
	guint fallback;

	for (fallback = 0; fallback < 2; fallback++) {
		testsuite_check ("Boundary and filename parameters round-trip without trimming [%u]", fallback);
		multipart = g_mime_multipart_new ();
		part = g_mime_part_new ();
		g_mime_multipart_set_boundary (multipart, " boundary");
		g_mime_part_set_filename (part, filename);
		if (fallback)
			g_mime_entity_set_content_disposition (GMIME_ENTITY (part), NULL);
		g_assert_cmpstr (g_mime_multipart_get_boundary (multipart), ==, " boundary");
		g_assert_cmpstr (g_mime_part_get_filename (part), ==, filename);
		g_mime_multipart_add (multipart, GMIME_ENTITY (part));
		stream = g_mime_stream_mem_new ();
		g_assert_cmpint (g_mime_entity_write_to_stream (GMIME_ENTITY (multipart), NULL, stream), >, 0);
		g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
		parser = g_mime_parser_new_with_stream (stream);
		parsed = g_mime_parser_construct_part (parser, NULL, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MULTIPART (parsed));
		g_assert_cmpstr (g_mime_multipart_get_boundary (GMIME_MULTIPART (parsed)), ==, " boundary");
		g_assert_cmpint (g_mime_multipart_get_count (GMIME_MULTIPART (parsed)), ==, 1);
		child = g_mime_multipart_get_part (GMIME_MULTIPART (parsed), 0);
		g_assert_true (GMIME_IS_PART (child));
		g_assert_cmpstr (g_mime_part_get_filename (GMIME_PART (child)), ==, filename);
		g_object_unref (parsed);
		g_object_unref (parser);
		g_object_unref (stream);
		g_object_unref (part);
		g_object_unref (multipart);
		testsuite_check_passed ();
	}
}

typedef GMimeCryptoContext WhitespaceCryptoContext;
typedef GMimeCryptoContextClass WhitespaceCryptoContextClass;

G_DEFINE_TYPE (WhitespaceCryptoContext, whitespace_crypto_context, GMIME_TYPE_CRYPTO_CONTEXT)

static const char *
whitespace_signature_protocol (GMimeCryptoContext *ctx)
{
	return "application/x-whitespace-signature";
}

static const char *
whitespace_encryption_protocol (GMimeCryptoContext *ctx)
{
	return "application/x-whitespace-encrypted";
}

static void
whitespace_crypto_context_class_init (WhitespaceCryptoContextClass *klass)
{
	klass->get_signature_protocol = whitespace_signature_protocol;
	klass->get_encryption_protocol = whitespace_encryption_protocol;
}

static void
whitespace_crypto_context_init (WhitespaceCryptoContext *ctx)
{
}

static GMimeCryptoContext *
whitespace_crypto_context_new (void)
{
	return g_object_new (whitespace_crypto_context_get_type (), NULL);
}

static void
test_protocol_whitespace (void)
{
	GMimeMultipartSigned *signed_part = g_mime_multipart_signed_new ();
	GMimeMultipartEncrypted *encrypted = g_mime_multipart_encrypted_new ();
	GMimePart *part = g_mime_part_new ();
	GError *error = NULL;
	char *value;
	const char *space;
	guint i;

	g_mime_crypto_context_register (whitespace_signature_protocol (NULL), whitespace_crypto_context_new);
	g_mime_crypto_context_register (whitespace_encryption_protocol (NULL), whitespace_crypto_context_new);
	g_mime_multipart_add (GMIME_MULTIPART (signed_part), GMIME_ENTITY (part));
	g_mime_multipart_add (GMIME_MULTIPART (signed_part), GMIME_ENTITY (part));
	g_mime_multipart_add (GMIME_MULTIPART (encrypted), GMIME_ENTITY (part));
	g_mime_multipart_add (GMIME_MULTIPART (encrypted), GMIME_ENTITY (part));
	for (i = 0; i <= G_N_ELEMENTS (padding); i++) {
		testsuite_check ("Crypto protocols accept only ASCII linear padding [%u]", i);
		space = i < G_N_ELEMENTS (padding) ? padding[i] : " \t\r\n";
		value = g_strconcat (space, whitespace_signature_protocol (NULL), space, NULL);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (signed_part), "protocol", value);
		g_assert_null (g_mime_multipart_signed_verify (signed_part, GMIME_VERIFY_NONE, &error));
		g_assert_error (error, GMIME_ERROR, (i < G_N_ELEMENTS (padding) ?
				GMIME_ERROR_PROTOCOL_ERROR : GMIME_ERROR_PARSE_ERROR));
		g_clear_error (&error);
		g_assert_cmpstr (g_mime_entity_get_content_type_parameter (GMIME_ENTITY (signed_part), "protocol"), ==, value);
		g_free (value);
		value = g_strconcat (space, whitespace_encryption_protocol (NULL), space, NULL);
		g_mime_entity_set_content_type_parameter (GMIME_ENTITY (encrypted), "protocol", value);
		g_assert_null (g_mime_multipart_encrypted_decrypt (encrypted, GMIME_DECRYPT_NONE, NULL, NULL, &error));
		g_assert_error (error, GMIME_ERROR, (i < G_N_ELEMENTS (padding) ?
				GMIME_ERROR_PROTOCOL_ERROR : GMIME_ERROR_PARSE_ERROR));
		g_clear_error (&error);
		g_assert_cmpstr (g_mime_entity_get_content_type_parameter (GMIME_ENTITY (encrypted), "protocol"), ==, value);
		g_free (value);
		testsuite_check_passed ();
	}
	g_object_unref (part);
	g_object_unref (encrypted);
	g_object_unref (signed_part);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME whitespace");
	test_header_whitespace ();
	test_token_whitespace ();
	test_address_whitespace ();
	test_parameter_whitespace ();
	test_protocol_whitespace ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
