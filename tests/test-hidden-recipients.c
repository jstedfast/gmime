/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "testsuite.h"
#include "test-crypto.h"

static const char plaintext[] = "Visible and hidden recipients";

static gpgme_key_t
generate_key (gpgme_ctx_t ctx, const char *name)
{
	char *parameters = g_strdup_printf (
		"<GnupgKeyParms format=\"internal\">\n"
		"Key-Type: RSA\nKey-Length: 2048\nKey-Usage: sign\n"
		"Subkey-Type: RSA\nSubkey-Length: 2048\nSubkey-Usage: encrypt\n"
		"Name-Real: %s\nName-Email: %s@example.invalid\n"
		"Expire-Date: 0\nPassphrase: no.secret\n</GnupgKeyParms>\n", name, name);
	char *fingerprint;
	gpgme_key_t key;

	g_assert_cmpuint (gpgme_op_genkey (ctx, parameters, NULL, NULL), ==, 0);
	g_free (parameters);
	fingerprint = g_strdup (gpgme_op_genkey_result (ctx)->fpr);
	key = test_crypto_key (ctx, fingerprint, TRUE);
	g_free (fingerprint);
	g_assert_nonnull (key->subkeys->next);
	g_assert_true (key->subkeys->next->can_encrypt);
	return key;
}

static void
check_recipients (gpgme_decrypt_result_t result, const char *visible_keyid, guint hidden_count)
{
	gpgme_recipient_t recipient;
	guint visible = 0, hidden = 0;

	g_assert_nonnull (result);
	for (recipient = result->recipients; recipient; recipient = recipient->next) {
		g_assert_nonnull (recipient->keyid);
		if (!strcmp (recipient->keyid, "0000000000000000")) {
			hidden++;
		} else {
			g_assert_nonnull (visible_keyid);
			g_assert_cmpstr (recipient->keyid, ==, visible_keyid);
			visible++;
		}
	}
	g_assert_cmpuint (visible, ==, visible_keyid ? 1 : 0);
	g_assert_cmpuint (hidden, ==, hidden_count);
}

static void
check_signature (gpgme_verify_result_t verified, gboolean sign)
{
	if (sign) {
		g_assert_nonnull (verified);
		g_assert_nonnull (verified->signatures);
		g_assert_null (verified->signatures->next);
		g_assert_cmpuint (verified->signatures->status, ==, 0);
	} else {
		g_assert_true (!verified || !verified->signatures);
	}
}

static void
decrypt_stream (gpgme_ctx_t ctx, GMimeStream *encrypted, gboolean sign,
		const char *visible_keyid, guint hidden_count)
{
	GMimeStream *output = g_mime_stream_mem_new ();
	gpgme_decrypt_result_t decrypted = NULL;
	gpgme_verify_result_t verified = NULL;
	GError *error = NULL;
	GByteArray *bytes;

	g_assert_cmpint (g_mime_stream_reset (encrypted), ==, 0);
	g_assert_true (g_mime_gpgme_decrypt (ctx, TRUE, encrypted, output, &decrypted, &verified, &error));
	g_assert_no_error (error);
	check_recipients (decrypted, visible_keyid, hidden_count);
	check_signature (verified, sign);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
	g_assert_cmpmem (bytes->data, bytes->len, plaintext, sizeof (plaintext) - 1);
	gpgme_result_unref (decrypted);
	g_clear_pointer (&verified, gpgme_result_unref);
	g_object_unref (output);
}

static void
test_streams (gpgme_ctx_t ctx, gpgme_key_t visible, gpgme_key_t hidden)
{
	gpgme_key_t recipients[] = { visible, NULL }, hidden_recipients[] = { hidden, NULL }, empty[] = { NULL };
	guint mode, sign;

	testsuite_check ("Native mixed, hidden-only, visible-only, throw-keyids and symmetric encryption");
	for (mode = 0; mode < 6; mode++) {
		for (sign = 0; sign < 2; sign++) {
			GMimeStream *input = g_mime_stream_mem_new_with_buffer (plaintext, sizeof (plaintext) - 1);
			GMimeStream *output = g_mime_stream_mem_new ();
			gpgme_encrypt_result_t encrypted = NULL;
			gpgme_sign_result_t signed_result = NULL;
			gpgme_encrypt_flags_t flags = GPGME_ENCRYPT_ALWAYS_TRUST | GPGME_ENCRYPT_NO_ENCRYPT_TO;
			gpgme_key_t *to = mode == 1 || mode == 5 ? NULL : recipients;
			gpgme_key_t *bcc = mode < 3 ? hidden_recipients : (mode == 4 ? empty : NULL);
			const char *keyid = to && mode != 2 ? visible->subkeys->next->keyid : NULL;
			GError *error = NULL;

			if (mode == 2)
				flags |= GPGME_ENCRYPT_THROW_KEYIDS;
			gpgme_set_armor (ctx, sign);
			gpgme_set_textmode (ctx, sign);
			g_assert_true (g_mime_gpgme_encrypt (ctx, sign, flags, to, bcc,
							    input, output, &encrypted, &signed_result, &error));
			g_assert_no_error (error);
			g_assert_cmpint (gpgme_get_armor (ctx), ==, sign);
			g_assert_cmpint (gpgme_get_textmode (ctx), ==, sign);
			decrypt_stream (ctx, output, sign, keyid, mode == 2 ? 2 : (mode < 2 ? 1 : 0));
			g_assert_nonnull (encrypted);
			g_assert_null (encrypted->invalid_recipients);
			if (sign) {
				g_assert_nonnull (signed_result);
				g_assert_cmpstr (signed_result->signatures->fpr, ==, visible->subkeys->fpr);
			} else {
				g_assert_null (signed_result);
			}
			gpgme_result_unref (encrypted);
			g_clear_pointer (&signed_result, gpgme_result_unref);
			g_object_unref (input);
			g_object_unref (output);
		}
	}
	testsuite_check_passed ();
}

static void
test_mime (gpgme_ctx_t ctx, gpgme_key_t visible, gpgme_key_t hidden)
{
	gpgme_key_t recipients[] = { visible, NULL }, hidden_recipients[] = { hidden, NULL };
	guint sign, hidden_only;

	testsuite_check ("PGP/MIME and inline encryption preserve hidden recipients and context settings");
	for (hidden_only = 0; hidden_only < 2; hidden_only++) {
		for (sign = 0; sign < 2; sign++) {
			GMimeTextPart *part = g_mime_text_part_new ();
			GMimeMultipartEncrypted *encrypted;
			GMimeEntity *decrypted;
			gpgme_decrypt_result_t result = NULL;
			gpgme_verify_result_t verified = NULL;
			gpgme_key_t *to = hidden_only ? NULL : recipients;
			const char *keyid = hidden_only ? NULL : visible->subkeys->next->keyid;
			GError *error = NULL;
			char *text;

			g_mime_text_part_set_text (part, plaintext);
			gpgme_set_armor (ctx, sign);
			gpgme_set_textmode (ctx, !sign);
			encrypted = g_mime_multipart_encrypted_encrypt (ctx, GMIME_ENTITY (part), sign,
				GPGME_ENCRYPT_ALWAYS_TRUST, to, hidden_recipients, NULL, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (encrypted);
			g_assert_cmpint (gpgme_get_armor (ctx), ==, sign);
			g_assert_cmpint (gpgme_get_textmode (ctx), ==, !sign);
			decrypted = g_mime_multipart_encrypted_decrypt (encrypted, ctx, &result, &verified, &error);
			g_assert_no_error (error);
			g_assert_true (GMIME_IS_TEXT_PART (decrypted));
			check_recipients (result, keyid, 1);
			check_signature (verified, sign);
			text = g_mime_text_part_get_text (GMIME_TEXT_PART (decrypted));
			g_assert_cmpstr (text, ==, plaintext);
			g_free (text);
			gpgme_result_unref (result);
			g_clear_pointer (&verified, gpgme_result_unref);
			g_object_unref (decrypted);
			g_object_unref (encrypted);

			g_assert_true (g_mime_part_openpgp_encrypt (GMIME_PART (part), ctx, sign,
				GPGME_ENCRYPT_ALWAYS_TRUST, to, hidden_recipients, NULL, NULL, &error));
			g_assert_no_error (error);
			g_assert_cmpint (gpgme_get_armor (ctx), ==, sign);
			g_assert_cmpint (gpgme_get_textmode (ctx), ==, !sign);
			g_assert_true (g_mime_part_openpgp_decrypt (GMIME_PART (part), ctx, &result, &verified, &error));
			g_assert_no_error (error);
			check_recipients (result, keyid, 1);
			check_signature (verified, sign);
			text = g_mime_text_part_get_text (part);
			g_assert_cmpstr (text, ==, plaintext);
			g_free (text);
			gpgme_result_unref (result);
			g_clear_pointer (&verified, gpgme_result_unref);
			g_object_unref (part);
			g_assert_cmpuint (gpgme_signers_count (ctx), ==, 1);
			g_assert_cmpint (gpgme_get_offline (ctx), ==, 1);
		}
	}
	testsuite_check_passed ();
}

static void
test_errors (gpgme_ctx_t ctx, gpgme_key_t key)
{
	struct _gpgme_key alias = *key;
	struct _gpgme_subkey subkey = *key->subkeys;
	gpgme_key_t recipients[] = { key, NULL }, hidden[] = { &alias, NULL };
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (plaintext, sizeof (plaintext) - 1);
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeTextPart *part = g_mime_text_part_new ();
	gpgme_ctx_t cms = test_crypto_context (GPGME_PROTOCOL_CMS);
	char *lowercase = g_ascii_strdown (subkey.fpr, -1);
	const char *invalid[] = { lowercase, NULL, "", "ABCD\n--no-hidden\n1234", "xyz", "1234",
				 "000000000000000000000000000000000000000G",
				 "000000000000000000000000000000000000000\n",
				 "00000000000000000000000000000000000000000000000000000000000000000" };
	GError *error = NULL;
	guint i;

	testsuite_check ("Reject overlapping, malformed and CMS recipients without changing content");
	alias.subkeys = &subkey;
	g_mime_text_part_set_text (part, plaintext);
	gpgme_set_armor (ctx, FALSE);
	gpgme_set_textmode (ctx, TRUE);
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		gpgme_encrypt_result_t encrypted = (gpgme_encrypt_result_t) GINT_TO_POINTER (1);
		gpgme_sign_result_t signed_result = (gpgme_sign_result_t) GINT_TO_POINTER (1);
		GMimeContent *saved = g_mime_part_get_content (GMIME_PART (part));
		char *text;

		subkey.fpr = (char *) invalid[i];
		g_assert_false (g_mime_gpgme_encrypt (ctx, TRUE, 0, recipients, hidden,
			input, output, &encrypted, &signed_result, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_assert_null (encrypted);
		g_assert_null (signed_result);
		g_clear_error (&error);
		g_assert_cmpint (g_mime_stream_tell (input), ==, 0);
		g_assert_cmpint (g_mime_stream_length (output), ==, 0);
		g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, 0, hidden, recipients,
			input, output, NULL, NULL, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_clear_error (&error);
		g_assert_null (g_mime_multipart_encrypted_encrypt (ctx, GMIME_ENTITY (part), FALSE,
			0, recipients, hidden, &encrypted, &signed_result, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_clear_error (&error);
		g_assert_false (g_mime_part_openpgp_encrypt (GMIME_PART (part), ctx, TRUE,
			0, recipients, hidden, &encrypted, &signed_result, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_assert_null (encrypted);
		g_assert_null (signed_result);
		g_clear_error (&error);
		g_assert_true (g_mime_part_get_content (GMIME_PART (part)) == saved);
		g_assert_cmpint (g_mime_part_get_openpgp_data (GMIME_PART (part)), ==, GMIME_OPENPGP_DATA_NONE);
		text = g_mime_text_part_get_text (part);
		g_assert_cmpstr (text, ==, plaintext);
		g_free (text);
		g_assert_cmpint (gpgme_get_armor (ctx), ==, FALSE);
		g_assert_cmpint (gpgme_get_textmode (ctx), ==, TRUE);
	}
	subkey.fpr = key->subkeys->fpr;
	g_assert_false (g_mime_gpgme_encrypt (cms, FALSE, 0, NULL, hidden,
		input, output, NULL, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_clear_error (&error);
	alias.protocol = GPGME_PROTOCOL_CMS;
	g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, 0, NULL, hidden,
		input, output, NULL, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	alias.protocol = GPGME_PROTOCOL_OpenPGP;
	alias.subkeys = NULL;
	g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, 0, NULL, hidden,
		input, output, NULL, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_clear_error (&error);
	{
		gpgme_key_t later[] = { key, &alias, NULL };

		g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, 0, NULL, later,
			input, output, NULL, NULL, &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
		g_clear_error (&error);
	}
	alias.subkeys = &subkey;
	subkey.fpr = "0000000000000000000000000000000000000001";
	{
		gpgme_encrypt_result_t encrypted = NULL;
		gpgme_key_t empty[] = { NULL };

		g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, GPGME_ENCRYPT_ALWAYS_TRUST,
			recipients, hidden, input, output, &encrypted, NULL, &error));
		g_assert_nonnull (error);
		g_assert_cmpuint (error->domain, ==, GMIME_GPGME_ERROR);
		g_assert_nonnull (encrypted);
		g_assert_nonnull (encrypted->invalid_recipients);
		g_clear_error (&error);
		/* Starting another operation must not invalidate retained diagnostics. */
		g_assert_false (g_mime_gpgme_encrypt (ctx, FALSE, 0, NULL, empty,
			input, output, NULL, NULL, &error));
		g_assert_nonnull (error);
		g_assert_cmpuint (error->domain, ==, GMIME_GPGME_ERROR);
		g_clear_error (&error);
		g_assert_nonnull (encrypted->invalid_recipients);
		gpgme_result_unref (encrypted);
	}
	g_free (lowercase);
	gpgme_release (cms);
	g_object_unref (part);
	g_object_unref (input);
	g_object_unref (output);
	testsuite_check_passed ();
}

static void
test_independent_decryption (gpgme_ctx_t ctx, gpgme_key_t visible, gpgme_key_t hidden)
{
	gpgme_key_t recipients[] = { visible, NULL }, hidden_recipients[] = { hidden, NULL };
	GMimeStream *input = g_mime_stream_mem_new_with_buffer (plaintext, sizeof (plaintext) - 1);
	GMimeStream *output = g_mime_stream_mem_new ();
	gpgme_data_t secret;
	GError *error = NULL;

	testsuite_check ("Each recipient alone can decrypt the same mixed-recipient ciphertext");
	gpgme_set_textmode (ctx, FALSE);
	g_assert_true (g_mime_gpgme_encrypt (ctx, FALSE, GPGME_ENCRYPT_ALWAYS_TRUST,
		recipients, hidden_recipients, input, output, NULL, NULL, &error));
	g_assert_no_error (error);

	/* Remove only generated keys in this test's isolated GnuPG home. */
	g_assert_cmpuint (gpgme_data_new (&secret), ==, 0);
	g_assert_cmpuint (gpgme_op_export (ctx, hidden->subkeys->fpr, GPGME_EXPORT_MODE_SECRET, secret), ==, 0);
	g_assert_cmpuint (gpgme_op_delete (ctx, hidden, TRUE), ==, 0);
	decrypt_stream (ctx, output, FALSE, visible->subkeys->next->keyid, 1);
	g_assert_cmpint (gpgme_data_seek (secret, 0, SEEK_SET), ==, 0);
	g_assert_cmpuint (gpgme_op_import (ctx, secret), ==, 0);
	gpgme_data_release (secret);
	g_assert_cmpuint (gpgme_op_delete (ctx, visible, TRUE), ==, 0);
	decrypt_stream (ctx, output, FALSE, visible->subkeys->next->keyid, 1);
	g_object_unref (input);
	g_object_unref (output);
	testsuite_check_passed ();
}

int
main (int argc, char **argv)
{
	gpgme_ctx_t ctx;
	gpgme_key_t visible, hidden;
	char *gpg;

	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("Hidden OpenPGP recipients");
	if (!(gpg = g_find_program_in_path ("gpg2")))
		gpg = g_find_program_in_path ("gpg");
	g_assert_nonnull (gpg);
	g_assert_cmpint (testsuite_setup_gpghome (gpg, "hidden-recipients"), ==, 0);
	g_free (gpg);
	ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);
	visible = generate_key (ctx, "visible");
	hidden = generate_key (ctx, "hidden");
	g_assert_cmpuint (gpgme_signers_add (ctx, visible), ==, 0);
	test_streams (ctx, visible, hidden);
	test_mime (ctx, visible, hidden);
	test_errors (ctx, visible);
	test_independent_decryption (ctx, visible, hidden);
	gpgme_key_unref (visible);
	gpgme_key_unref (hidden);
	gpgme_release (ctx);
	g_assert_cmpint (testsuite_destroy_gpghome (), ==, 0);
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
