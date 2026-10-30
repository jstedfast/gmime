/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2026 Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <gmime/gmime.h>
#include "testsuite.h"
#ifdef ENABLE_CRYPTO
#include "test-crypto.h"
#endif

#define MESSAGE "-----BEGIN PGP MESSAGE-----\nabc\n-----END PGP MESSAGE-----\n"
#define SIGNED "-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n\ntext\n-----BEGIN PGP SIGNATURE-----\nabc\n-----END PGP SIGNATURE-----\n"
#define PUBLIC_KEY "-----BEGIN PGP PUBLIC KEY BLOCK-----\nabc\n-----END PGP PUBLIC KEY BLOCK-----\n"
#define PRIVATE_KEY "-----BEGIN PGP PRIVATE KEY BLOCK-----\nabc\n-----END PGP PRIVATE KEY BLOCK-----\n"
#define SIGNATURE "-----BEGIN PGP SIGNATURE-----\nabc\n-----END PGP SIGNATURE-----\n"

static GMimePart *
make_part (const char *bytes, gsize length, GMimeContentEncoding encoding, gboolean onebyte)
{
	GMimeStream *stream = g_mime_stream_mem_new_with_buffer (bytes, length);
	GMimeContent *content;
	GMimePart *part;

	if (onebyte) {
		GMimeStream *wrapper = test_stream_onebyte_new (stream);
		g_object_unref (stream);
		stream = wrapper;
	}
	content = g_mime_content_new_with_stream (stream, encoding);
	part = g_mime_part_new_with_type ("text", "plain");
	g_mime_part_set_content (part, content);
	g_object_unref (content);
	g_object_unref (stream);
	return part;
}

static GMimeOpenPGPDocument *
scan_part (GMimePart *part)
{
	GError *error = NULL;
	GMimeOpenPGPDocument *document = g_mime_part_scan_openpgp (part, &error);

	g_assert_no_error (error);
	g_assert_nonnull (document);
	return document;
}

static void
assert_stream (GMimeStream *stream, const char *expected, gsize length)
{
	char buffer[4096];
	gsize position = 0;
	ssize_t nread;

	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	while ((nread = g_mime_stream_read (stream, buffer, sizeof (buffer))) > 0) {
		g_assert_cmpuint (position + nread, <=, length);
		g_assert_cmpmem (buffer, nread, expected + position, nread);
		position += nread;
	}
	g_assert_cmpint (nread, ==, 0);
	g_assert_true (g_mime_stream_eos (stream));
	g_assert_cmpuint (position, ==, length);
}

static void
test_discovery (void)
{
	const char source[] = "prefix\0bytes\n" SIGNED "gap\n" MESSAGE PUBLIC_KEY PRIVATE_KEY SIGNATURE "suffix";
	const char *blocks[] = { SIGNED, MESSAGE, PUBLIC_KEY, PRIVATE_KEY, SIGNATURE };
	const GMimeOpenPGPBlockKind kinds[] = {
		GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE, GMIME_OPENPGP_BLOCK_MESSAGE,
		GMIME_OPENPGP_BLOCK_PUBLIC_KEY, GMIME_OPENPGP_BLOCK_PRIVATE_KEY, GMIME_OPENPGP_BLOCK_SIGNATURE
	};
	GMimeOpenPGPDocument *document;
	GMimeOpenPGPBlock *block;
	GMimeStream *stream;
	GMimePart *part;
	gsize offset;
	guint i, j;
	char *encoded;

	testsuite_check ("Ordered blocks, decoded offsets, binary gaps, and one-byte reads");
	g_assert_true (g_type_test_flags (GMIME_TYPE_OPENPGP_DOCUMENT, G_TYPE_FLAG_FINAL));
	g_assert_true (g_type_test_flags (GMIME_TYPE_OPENPGP_BLOCK, G_TYPE_FLAG_FINAL));
	for (j = 0; j < 3; j++) {
		encoded = j == 2 ? g_base64_encode ((const guchar *) source, sizeof (source) - 1) : NULL;
		part = make_part (encoded ? encoded : source, encoded ? strlen (encoded) : sizeof (source) - 1,
				  encoded ? GMIME_CONTENT_ENCODING_BASE64 : GMIME_CONTENT_ENCODING_UNSPECIFIED, j != 0);
		document = scan_part (part);
		g_assert_cmpuint (g_mime_openpgp_document_get_count (document), ==, G_N_ELEMENTS (blocks));
		g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (document)), ==, G_N_ELEMENTS (blocks));
		g_assert_cmpuint (g_list_model_get_item_type (G_LIST_MODEL (document)), ==, GMIME_TYPE_OPENPGP_BLOCK);
		g_assert_cmpint (g_mime_openpgp_document_get_length (document), ==, sizeof (source) - 1);
		g_assert_null (g_mime_openpgp_document_get_block (document, 5));
		g_assert_null (g_list_model_get_item (G_LIST_MODEL (document), 5));
		offset = sizeof ("prefix\0bytes\n") - 1;
		for (i = 0; i < G_N_ELEMENTS (blocks); i++) {
			block = g_list_model_get_item (G_LIST_MODEL (document), i);
			g_assert_true (block == g_mime_openpgp_document_get_block (document, i));
			g_assert_cmpint (g_mime_openpgp_block_get_kind (block), ==, kinds[i]);
			g_assert_cmpint (g_mime_openpgp_block_get_status (block), ==, GMIME_OPENPGP_BLOCK_COMPLETE);
			g_assert_cmpint (g_mime_openpgp_block_get_offset (block), ==, offset);
			g_assert_cmpint (g_mime_openpgp_block_get_length (block), ==, strlen (blocks[i]));
			stream = g_mime_openpgp_block_open_stream (block);
			g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
			assert_stream (stream, blocks[i], strlen (blocks[i]));
			g_object_unref (stream);
			g_object_unref (block);
			offset += strlen (blocks[i]) + (i == 0 ? strlen ("gap\n") : 0);
		}
		stream = g_mime_openpgp_document_open_stream (document);
		assert_stream (stream, source, sizeof (source) - 1);
		g_object_unref (stream);
		g_object_unref (document);
		g_object_unref (part);
		g_free (encoded);
	}
	testsuite_check_passed ();
}

static void
test_recovery (void)
{
	const struct {
		const char *text;
		guint count;
		GMimeOpenPGPBlockStatus first;
		gsize first_length;
	} cases[] = {
		{ "", 0, 0, 0 },
		{ "> " MESSAGE, 0, 0, 0 },
		{ "-----BEGIN PGP MESSAGE----- \nabc\n-----END PGP MESSAGE-----\n", 0, 0, 0 },
		{ "-----BEGIN PGP MESSAGE-----\rabc\r-----END PGP MESSAGE-----", 0, 0, 0 },
		{ "-----BEGIN PGP MESSAGE-----", 1, GMIME_OPENPGP_BLOCK_TRUNCATED, 27 },
		{ "-----BEGIN PGP MESSAGE-----\r\nabc\r\n-----END PGP MESSAGE-----", 1, GMIME_OPENPGP_BLOCK_COMPLETE, 59 },
		{ "-----BEGIN PGP MESSAGE-----\nabc\n" SIGNED, 2, GMIME_OPENPGP_BLOCK_INTERRUPTED, 32 },
		{ "-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n\ntext\n" MESSAGE, 2, GMIME_OPENPGP_BLOCK_INTERRUPTED, 54 },
		{ "-----BEGIN PGP MESSAGE-----\n-----END PGP SIGNATURE-----\n" MESSAGE, 2, GMIME_OPENPGP_BLOCK_MALFORMED, 56 },
		{ SIGNATURE "-----BEGIN PGP MESSAGE-----\n", 2, GMIME_OPENPGP_BLOCK_COMPLETE, sizeof (SIGNATURE) - 1 },
		{ "- -----BEGIN PGP MESSAGE-----\n", 0, 0, 0 },
		{ "-----BEGIN PGP MESSAGE-----\r", 0, 0, 0 },
		{ "-----BEGIN PGP MESSAGE-----\nabc\n-----END PGP MESSAGE-----\r",
		  1, GMIME_OPENPGP_BLOCK_TRUNCATED, 58 },
	};
	GMimeOpenPGPDocument *document;
	GMimeOpenPGPBlock *block;
	GMimePart *part;
	GString *source;
	guint i;

	testsuite_check ("Malformed recovery, strict markers, LF/CRLF, EOF, and chunk boundaries");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		part = make_part (cases[i].text, strlen (cases[i].text), GMIME_CONTENT_ENCODING_UNSPECIFIED, TRUE);
		document = scan_part (part);
		g_assert_cmpuint (g_mime_openpgp_document_get_count (document), ==, cases[i].count);
		if (cases[i].count) {
			block = g_mime_openpgp_document_get_block (document, 0);
			g_assert_cmpint (g_mime_openpgp_block_get_status (block), ==, cases[i].first);
			g_assert_cmpint (g_mime_openpgp_block_get_length (block), ==, cases[i].first_length);
		}
		if (cases[i].count == 2 && i != 9) {
			block = g_mime_openpgp_document_get_block (document, 1);
			g_assert_cmpint (g_mime_openpgp_block_get_status (block), ==, GMIME_OPENPGP_BLOCK_COMPLETE);
			g_assert_cmpint (g_mime_openpgp_block_get_offset (block), ==, cases[i].first_length);
		}
		g_object_unref (document);
		g_object_unref (part);
	}
	for (i = 0; i < 80; i++) {
		source = g_string_sized_new (4200);
		g_string_set_size (source, 4050 + i);
		memset (source->str, 'x', source->len);
		g_string_append (source, "\n" MESSAGE);
		part = make_part (source->str, source->len, GMIME_CONTENT_ENCODING_UNSPECIFIED, FALSE);
		document = scan_part (part);
		g_assert_cmpuint (g_mime_openpgp_document_get_count (document), ==, 1);
		block = g_mime_openpgp_document_get_block (document, 0);
		g_assert_cmpint (g_mime_openpgp_block_get_offset (block), ==, 4051 + i);
		g_assert_cmpint (g_mime_openpgp_block_get_length (block), ==, sizeof (MESSAGE) - 1);
		g_object_unref (document);
		g_object_unref (part);
		g_string_free (source, TRUE);
	}
	testsuite_check_passed ();
}

static guint
snapshot_files (const char *directory)
{
	GError *error = NULL;
	GDir *dir = g_dir_open (directory, 0, &error);
	const char *name;
	guint count = 0;

	g_assert_no_error (error);
	while ((name = g_dir_read_name (dir))) {
		if (g_str_has_prefix (name, "gmime-openpgp-"))
			count++;
	}
	g_dir_close (dir);
	return count;
}

static void
test_storage (const char *directory)
{
	const gsize sizes[] = { 1024 * 1024 - 1, 1024 * 1024, 1024 * 1024 + 1, 2 * 1024 * 1024 + 97 };
	GMimeOpenPGPDocument *document;
	GMimeOpenPGPBlock *block;
	GMimeStream *source, *first, *second, *sub;
	GMimePart *part;
	GString *text;
	char buffer[6];
	guint i;

	testsuite_check ("Spill threshold, immutable storage, independent views, and temporary-file lifetime");
	for (i = 0; i < G_N_ELEMENTS (sizes); i++) {
		text = g_string_sized_new (sizes[i]);
		g_string_set_size (text, sizes[i] - sizeof (MESSAGE));
		memset (text->str, 'x', text->len);
		g_string_append (text, "\n" MESSAGE);
		g_assert_cmpuint (text->len, ==, sizes[i]);
		part = make_part (text->str, text->len, GMIME_CONTENT_ENCODING_UNSPECIFIED, FALSE);
		document = scan_part (part);
		g_assert_cmpuint (snapshot_files (directory), ==, sizes[i] > 1024 * 1024 ? 1 : 0);
		block = g_list_model_get_item (G_LIST_MODEL (document), 0);
		source = g_mime_openpgp_document_open_stream (document);
		first = g_mime_openpgp_block_open_stream (block);
		second = g_mime_openpgp_block_open_stream (block);
		sub = g_mime_stream_substream (first, 5, 10);
		g_mime_part_set_content (part, NULL);
		g_object_unref (part);
		g_object_unref (document);
		g_assert_cmpint (g_mime_stream_read (first, buffer, 3), ==, 3);
		g_assert_cmpint (g_mime_stream_read (second, buffer, 5), ==, 5);
		g_assert_cmpmem (buffer, 5, "-----", 5);
		g_assert_cmpint (g_mime_stream_tell (first), ==, 3);
		g_assert_cmpint (g_mime_stream_write (first, "x", 1), ==, -1);
		g_assert_cmpint (errno, ==, EBADF);
		g_assert_cmpint (g_mime_stream_seek (first, G_MININT64, GMIME_STREAM_SEEK_CUR), ==, -1);
		g_assert_cmpint (g_mime_stream_seek (first, G_MAXINT64, GMIME_STREAM_SEEK_END), ==, -1);
		assert_stream (second, MESSAGE, sizeof (MESSAGE) - 1);
		assert_stream (source, text->str, text->len);
		g_assert_cmpint (g_mime_stream_close (first), ==, 0);
		g_assert_cmpint (g_mime_stream_read (first, buffer, 1), ==, -1);
		g_assert_cmpint (errno, ==, EBADF);
		assert_stream (sub, "BEGIN", 5);
		g_assert_cmpint (g_mime_stream_seek (sub, 0, GMIME_STREAM_SEEK_SET), ==, -1);
		g_assert_cmpint (g_mime_stream_reset (sub), ==, 0);
		g_assert_cmpint (g_mime_stream_tell (sub), ==, 5);
		g_object_unref (first);
		g_object_unref (second);
		g_object_unref (source);
		g_assert_cmpuint (snapshot_files (directory), ==, sizes[i] > 1024 * 1024 ? 1 : 0);
		g_object_unref (sub);
		g_assert_cmpuint (snapshot_files (directory), ==, sizes[i] > 1024 * 1024 ? 1 : 0);
		first = g_mime_openpgp_block_open_stream (block);
		g_object_unref (block);
		assert_stream (first, MESSAGE, sizeof (MESSAGE) - 1);
		g_object_unref (first);
		g_assert_cmpuint (snapshot_files (directory), ==, 0);
		g_string_free (text, TRUE);
	}
	testsuite_check_passed ();
}

typedef struct {
	GMimeStreamMem parent_object;
	gint64 fail_at;
	gboolean stall;
} FaultStream;
typedef GMimeStreamMemClass FaultStreamClass;
G_DEFINE_TYPE (FaultStream, fault_stream, GMIME_TYPE_STREAM_MEM)

static ssize_t
fault_read (GMimeStream *stream, char *buffer, size_t length)
{
	FaultStream *self = (FaultStream *) stream;
	gint64 position = g_mime_stream_tell (stream);

	if (position >= self->fail_at) {
		errno = 0;
		return self->stall ? 0 : -1;
	}
	return GMIME_STREAM_CLASS (fault_stream_parent_class)->read (stream, buffer,
								    MIN (length, (gsize) (self->fail_at - position)));
}

static void
fault_stream_class_init (FaultStreamClass *klass)
{
	GMIME_STREAM_CLASS (klass)->read = fault_read;
}

static void
fault_stream_init (FaultStream *stream)
{
}

static void
test_errors (const char *directory)
{
	GError *error = NULL;
	GMimePart *part = g_mime_part_new ();
	GMimeContent *content;
	FaultStream *fault;
	GByteArray *data;
	guint i;

	testsuite_check ("Missing content, rewind/read failures, stalls, and partial-spill cleanup");
	g_assert_null (g_mime_part_scan_openpgp (part, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	for (i = 0; i < 3; i++) {
		data = g_byte_array_sized_new (2 * 1024 * 1024);
		g_byte_array_set_size (data, 2 * 1024 * 1024);
		memset (data->data, 'x', data->len);
		fault = g_object_new (fault_stream_get_type (), NULL);
		fault->fail_at = i == 0 ? 0 : 1024 * 1024 + 7;
		fault->stall = i == 2;
		g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (fault), data);
		g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (fault), TRUE);
		content = g_mime_content_new_with_stream (GMIME_STREAM (fault), GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content (part, content);
		g_object_unref (content);
		g_assert_null (g_mime_part_scan_openpgp (part, &error));
		g_assert_error (error, GMIME_ERROR, EIO);
		g_clear_error (&error);
		g_assert_cmpuint (snapshot_files (directory), ==, 0);
		g_mime_stream_close (GMIME_STREAM (fault));
		g_assert_null (g_mime_part_scan_openpgp (part, &error));
		g_assert_nonnull (error);
		g_clear_error (&error);
		g_object_unref (fault);
	}
	g_object_unref (part);
	testsuite_check_passed ();
}

static void
test_spill_failure (void)
{
	GError *error = NULL;
	char *text = g_malloc0 (1024 * 1024 + 1);
	GMimePart *part = make_part (text, 1024 * 1024 + 1, GMIME_CONTENT_ENCODING_UNSPECIFIED, FALSE);

	g_assert_null (g_mime_part_scan_openpgp (part, &error));
	g_assert_nonnull (error);
	g_clear_error (&error);
	g_object_unref (part);
	g_free (text);
}

#ifdef ENABLE_CRYPTO
static void
append_stream (GString *text, GMimeStream *stream)
{
	GByteArray *data = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));

	g_string_append_len (text, (const char *) data->data, data->len);
}

static GMimeStream *
sign_text (gpgme_ctx_t ctx, const char *text, gpgme_sig_mode_t mode)
{
	GError *error = NULL;
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (text, strlen (text));
	GMimeStream *output = g_mime_stream_mem_new ();

	g_assert_true (g_mime_gpgme_sign (ctx, mode, input, output, NULL, &error));
	g_assert_no_error (error);
	g_object_unref (input);
	return output;
}

static void
test_crypto (const char *datadir)
{
	GMimeOpenPGPDocument *document;
	GMimeOpenPGPBlock *block;
	gpgme_verify_result_t verified[7] = { NULL };
	gpgme_decrypt_result_t decrypted = NULL;
	gpgme_key_t first_key, second_key, recipients[2];
	gpgme_ctx_t ctx, cms;
	GMimePart *part;
	GMimeContent *content;
	GMimeStream *input, *output, *signed_text;
	GString *text = g_string_new ("unprotected prefix\n");
	GError *error = NULL;
	char *gpg, *path, *tampered;
	guint i, count;
	gpgme_signature_t signature;
	const char *expected[] = { "first payload\n", NULL, "encrypted payload\n", "second payload\n",
				  "first payload\n", "two signers\n", "opaque payload\n" };

	testsuite_check ("Independent native results, invalid and missing keys, and non-mutating receiving");
	if (!(gpg = g_find_program_in_path ("gpg2")))
		gpg = g_find_program_in_path ("gpg");
	g_assert_nonnull (gpg);
	g_assert_cmpint (testsuite_setup_gpghome (gpg, "inline-pgp"), ==, 0);
	ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);
	for (i = 0; i < 2; i++) {
		path = g_build_filename (datadir, i ? "gmime.gpg.sec" : "gmime.gpg.pub", NULL);
		input = g_mime_stream_fs_open (path, O_RDONLY, 0, &error);
		g_assert_no_error (error);
		test_crypto_import (ctx, input, &error);
		g_assert_no_error (error);
		g_object_unref (input);
		g_free (path);
	}
	first_key = test_crypto_key (ctx, "no.user@no.domain", TRUE);
	g_assert_cmpuint (gpgme_op_genkey (ctx,
		"<GnupgKeyParms format=\"internal\">\nKey-Type: RSA\nKey-Length: 1024\nKey-Usage: sign\n"
		"Name-Real: Inline PGP Second Signer\nName-Email: inline-second@example.invalid\n"
		"Expire-Date: 0\nPassphrase: no.secret\n</GnupgKeyParms>\n", NULL, NULL), ==, 0);
	path = g_strdup (gpgme_op_genkey_result (ctx)->fpr);
	second_key = test_crypto_key (ctx, path, TRUE);
	g_free (path);
	gpgme_set_armor (ctx, TRUE);
	g_assert_cmpuint (gpgme_signers_add (ctx, first_key), ==, 0);
	signed_text = sign_text (ctx, expected[0], GPGME_SIG_MODE_CLEAR);
	append_stream (text, signed_text);
	g_string_append (text, "unprotected gap\n" MESSAGE);
	recipients[0] = first_key;
	recipients[1] = NULL;
	input = g_mime_stream_mem_new_with_buffer (expected[2], strlen (expected[2]));
	output = g_mime_stream_mem_new ();
	g_assert_true (g_mime_gpgme_encrypt (ctx, TRUE, GPGME_ENCRYPT_ALWAYS_TRUST, recipients,
					    input, output, NULL, NULL, &error));
	g_assert_no_error (error);
	append_stream (text, output);
	g_object_unref (input);
	g_object_unref (output);
	gpgme_signers_clear (ctx);
	g_assert_cmpuint (gpgme_signers_add (ctx, second_key), ==, 0);
	output = sign_text (ctx, expected[3], GPGME_SIG_MODE_CLEAR);
	append_stream (text, output);
	g_object_unref (output);
	append_stream (text, signed_text);
	tampered = g_strrstr (text->str, expected[0]);
	g_assert_nonnull (tampered);
	*tampered = 'F';
	g_object_unref (signed_text);
	g_assert_cmpuint (gpgme_signers_add (ctx, first_key), ==, 0);
	output = sign_text (ctx, expected[5], GPGME_SIG_MODE_CLEAR);
	append_stream (text, output);
	g_object_unref (output);
	output = sign_text (ctx, expected[6], GPGME_SIG_MODE_NORMAL);
	append_stream (text, output);
	g_object_unref (output);
	g_string_append (text, "unprotected suffix\n" PUBLIC_KEY "-----BEGIN PGP MESSAGE-----\n");
	part = make_part (text->str, text->len, GMIME_CONTENT_ENCODING_UNSPECIFIED, FALSE);
	content = g_mime_part_get_content (part);
	document = scan_part (part);
	g_assert_cmpuint (g_mime_openpgp_document_get_count (document), ==, 9);
	gpgme_set_armor (ctx, FALSE);
	gpgme_set_textmode (ctx, TRUE);
	for (i = 0; i < 7; i++) {
		block = g_mime_openpgp_document_get_block (document, i);
		output = g_mime_stream_mem_new ();
		if (i == 1) {
			g_assert_false (g_mime_openpgp_block_decrypt (block, ctx, output, NULL, NULL, &error));
			g_assert_nonnull (error);
			g_clear_error (&error);
		} else if (i == 2) {
			g_assert_true (g_mime_openpgp_block_decrypt (block, ctx, output, &decrypted, &verified[i], &error));
			g_assert_nonnull (decrypted);
		} else {
			g_assert_true (g_mime_openpgp_block_verify (block, ctx, output, &verified[i], &error));
		}
		g_assert_no_error (error);
		if (i != 1) {
			g_assert_nonnull (verified[i]);
			g_assert_nonnull (verified[i]->signatures);
			if (i == 4) {
				g_assert_cmpuint (gpgme_err_code (verified[i]->signatures->status), ==, GPG_ERR_BAD_SIGNATURE);
			} else {
				for (signature = verified[i]->signatures, count = 0; signature; signature = signature->next, count++)
					g_assert_cmpuint (signature->status, ==, 0);
				g_assert_cmpuint (count, ==, i >= 5 ? 2 : 1);
				assert_stream (output, expected[i], strlen (expected[i]));
			}
		}
		g_assert_cmpint (gpgme_get_armor (ctx), ==, FALSE);
		g_assert_cmpint (gpgme_get_textmode (ctx), ==, TRUE);
		g_assert_cmpuint (gpgme_signers_count (ctx), ==, 2);
		g_assert_true (content == g_mime_part_get_content (part));
		g_object_unref (output);
	}
	g_assert_cmpstr (verified[0]->signatures->fpr, !=, verified[3]->signatures->fpr);
	output = g_mime_openpgp_document_open_stream (document);
	assert_stream (output, text->str, text->len);
	g_object_unref (output);
	output = g_mime_stream_mem_new ();
	cms = test_crypto_context (GPGME_PROTOCOL_CMS);
	block = g_mime_openpgp_document_get_block (document, 0);
	g_assert_false (g_mime_openpgp_block_verify (block, cms, output, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_clear_error (&error);
	gpgme_release (cms);
	for (i = 7; i < 9; i++) {
		gpgme_verify_result_t result = verified[0];
		gpgme_decrypt_result_t decrypt_result = decrypted;
		block = g_mime_openpgp_document_get_block (document, i);
		g_assert_false (g_mime_openpgp_block_verify (block, ctx, output, &result, &error));
		g_assert_error (error, GMIME_ERROR, (i == 7 ? GMIME_ERROR_NOT_SUPPORTED : GMIME_ERROR_PARSE_ERROR));
		g_assert_null (result);
		g_clear_error (&error);
		result = verified[0];
		g_assert_false (g_mime_openpgp_block_decrypt (block, ctx, output, &decrypt_result, &result, &error));
		g_assert_error (error, GMIME_ERROR, (i == 7 ? GMIME_ERROR_NOT_SUPPORTED : GMIME_ERROR_PARSE_ERROR));
		g_assert_null (decrypt_result);
		g_assert_null (result);
		g_clear_error (&error);
	}
	gpgme_key_unref (first_key);
	gpgme_key_unref (second_key);
	gpgme_release (ctx);
	g_assert_cmpint (testsuite_destroy_gpghome (), ==, 0);
	g_assert_cmpint (testsuite_setup_gpghome (gpg, "inline-pgp-missing"), ==, 0);
	ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);
	block = g_mime_openpgp_document_get_block (document, 3);
	{
		gpgme_verify_result_t result = NULL;
		g_assert_true (g_mime_openpgp_block_verify (block, ctx, output, &result, &error));
		g_assert_no_error (error);
		g_assert_nonnull (result);
		g_assert_cmpuint (gpgme_err_code (result->signatures->status), ==, GPG_ERR_NO_PUBKEY);
		gpgme_result_unref (result);
	}
	gpgme_release (ctx);
	g_assert_cmpuint (verified[0]->signatures->status, ==, 0);
	g_assert_nonnull (decrypted->recipients);
	gpgme_result_unref (decrypted);
	for (i = 0; i < 7; i++)
		g_clear_pointer (&verified[i], gpgme_result_unref);
	g_object_unref (output);
	g_object_unref (document);
	g_object_unref (part);
	g_string_free (text, TRUE);
	g_assert_cmpint (testsuite_destroy_gpghome (), ==, 0);
	g_free (gpg);
	testsuite_check_passed ();
}
#endif

int
main (int argc, char **argv)
{
	char *directory, *cwd, **env;
	char *child_argv[] = { argv[0], "--spill-failure", NULL };
	GError *error = NULL;
	int status;

	if (argc == 2 && !strcmp (argv[1], "--spill-failure")) {
		g_mime_init ();
		test_spill_failure ();
		g_mime_shutdown ();
		return 0;
	}
	cwd = g_get_current_dir ();
	directory = g_build_filename (cwd, "openpgp-tmp-XXXXXX", NULL);
	g_free (cwd);
	g_assert_nonnull (g_mkdtemp (directory));
	g_setenv ("TMPDIR", directory, TRUE);
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Multi-block inline OpenPGP");
	test_discovery ();
	test_recovery ();
	test_storage (directory);
	test_errors (directory);
	env = g_get_environ ();
	cwd = g_build_filename (directory, "missing", NULL);
	env = g_environ_setenv (env, "TMPDIR", cwd, TRUE);
	g_free (cwd);
	testsuite_check ("Temporary-file creation failure is explicit");
	g_assert_true (g_spawn_sync (NULL, child_argv, env, 0, NULL, NULL, NULL, NULL, &status, &error));
	g_assert_no_error (error);
	g_assert_true (g_spawn_check_wait_status (status, &error));
	g_assert_no_error (error);
	g_strfreev (env);
	testsuite_check_passed ();
#ifdef ENABLE_CRYPTO
	test_crypto (argc > 2 ? argv[argc - 1] : "data/pgp");
#endif
	testsuite_end ();
	g_mime_shutdown ();
	g_assert_cmpuint (snapshot_files (directory), ==, 0);
	g_assert_cmpint (g_rmdir (directory), ==, 0);
	g_free (directory);
	return testsuite_exit ();
}
