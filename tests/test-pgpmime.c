/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#include <gmime/gmime-object-private.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"
#ifdef ENABLE_CRYPTO
#include "test-crypto.h"
#endif

extern int verbose;

#define v(x) if (verbose > 3) x

#ifdef ENABLE_CRYPTO
static GMimeMessage *
create_message (GMimeEntity *body)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeInternetAddressList *list;
	GMimeInternetAddress *mailbox;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	
	message = g_mime_message_new (TRUE);
	
	mailbox = g_mime_mailbox_address_new ("Jeffrey Stedfast", "fejj@helixcode.com", NULL);
	list = g_mime_message_get_from (message);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = g_mime_mailbox_address_new ("Jeffrey Stedfast", "fejj@helixcode.com", NULL);
	list = g_mime_message_get_reply_to (message);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = g_mime_mailbox_address_new ("Federico Mena-Quintero", "federico@helixcode.com", NULL);
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	g_mime_message_set_subject (message, "This is a test message", NULL);
	g_mime_entity_set_header ((GMimeEntity *) message, "X-Mailer", "main.c", NULL);
	g_mime_message_set_mime_part (message, body);
	
	stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream ((GMimeEntity *) message, format, stream);
	g_mime_stream_reset (stream);
	g_object_unref (message);
	
	/*GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (stream));
	fprintf (stderr, "-----BEGIN RAW MESSAGE-----\n%.*s-----END RAW MESSAGE-----\n",
		 priv->buffer->len, priv->buffer->data);*/
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser, NULL, NULL);
	g_object_unref (parser);
	
	return message;
}

#define MULTIPART_SIGNED_CONTENT "This is a test of the emergency broadcast system \
with an sha1 detach-sign.\n\nFrom now on, there will be text to try and break     \t\
  \nvarious things. For example, the F in \"From\" in the previous line...\n...and \
the first dot of this line have been pre-encoded in the QP encoding in order to test \
that GMime properly treats MIME part content as opaque.\nIf this still verifies okay, \
then we have ourselves a winner I guess...\n"

static void
test_multipart_signed (gpgme_ctx_t ctx)
{
	GMimeMessagePrivate *priv;
	gpgme_verify_result_t signatures = NULL;
	gpgme_sigsum_t status;
	GMimeMultipartSigned *mps;
	GMimeMessage *message;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	
	part = g_mime_text_part_new_with_subtype ("plain");
	g_mime_text_part_set_text (part, MULTIPART_SIGNED_CONTENT);
	
	/* sign the part */
	mps = g_mime_multipart_signed_sign (ctx, (GMimeEntity *) part, NULL, &err);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("signing failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeEntity *) mps);
	g_object_unref (mps);
	
	priv = _g_mime_message_get_private (message);

	if (!GMIME_IS_MULTIPART_SIGNED (priv->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/signed?");
		g_object_unref (message);
		throw (ex);
	}
	
	mps = (GMimeMultipartSigned *) priv->mime_part;
	
	if (!g_mime_multipart_signed_verify (mps, ctx, &signatures, &err)) {
		g_clear_pointer (&signatures, gpgme_result_unref);
		ex = exception_new ("%s", err->message);
		g_object_unref (message);
		v(fputs ("failed.\n", stdout));
		g_error_free (err);
		throw (ex);
	}
	
	v(print_verify_results (signatures));
	
	status = get_sig_status (signatures);
	gpgme_result_unref (signatures);
	g_object_unref (message);
	
	if (status & GPGME_SIGSUM_RED)
		throw (exception_new ("signature status was BAD"));
}

#define MULTIPART_ENCRYPTED_CONTENT "This is a test of multipart/encrypted.\n"

static void
create_encrypted_message (gpgme_ctx_t ctx, gboolean sign,
			  GMimeStream **cleartext_out, GMimeStream **stream_out)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeStream *cleartext, *stream;
	GMimeMultipartEncrypted *mpe;
	GMimeInternetAddressList *list;
	GMimeInternetAddress *mailbox;
	gpgme_key_t recipients[2] = { NULL, NULL };
	GMimeMessage *message;
	Exception *ex = NULL;
	GError *err = NULL;
	GMimeTextPart *part;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, MULTIPART_ENCRYPTED_CONTENT);
	
	/* hold onto this for comparison later */
	cleartext = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream ((GMimeEntity *) part, format, cleartext);
	g_mime_stream_reset (cleartext);
	
	/* encrypt the part */
	recipients[0] = test_crypto_key (ctx, "no.user@no.domain", FALSE);
	mpe = g_mime_multipart_encrypted_encrypt (ctx, (GMimeEntity *) part, sign,
						  GPGME_ENCRYPT_ALWAYS_TRUST, recipients, NULL, NULL, &err);
	gpgme_key_unref (recipients[0]);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("encryption failed: %s", err->message);
		g_object_unref (cleartext);
		g_error_free (err);
		throw (ex);
	}
	
	message = g_mime_message_new (TRUE);
	
	mailbox = g_mime_mailbox_address_new ("Jeffrey Stedfast", "fejj@helixcode.com", NULL);
	list = g_mime_message_get_from (message);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = g_mime_mailbox_address_new ("Jeffrey Stedfast", "fejj@helixcode.com", NULL);
	list = g_mime_message_get_reply_to (message);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = g_mime_mailbox_address_new ("Federico Mena-Quintero", "federico@helixcode.com", NULL);
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	g_mime_internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	g_mime_message_set_subject (message, "This is a test message", NULL);
	g_mime_entity_set_header ((GMimeEntity *) message, "X-Mailer", "main.c", NULL);
	g_mime_message_set_mime_part (message, (GMimeEntity *) mpe);
	g_object_unref (mpe);
	
	stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream ((GMimeEntity *) message, format, stream);
	g_object_unref (message);
	
	*stream_out = stream;
	*cleartext_out = cleartext;
}	

static char *
test_multipart_encrypted (gpgme_ctx_t ctx, gboolean sign,
			  GMimeStream *cleartext, GMimeStream *stream,
			  const char *session_key)
{
	GMimeMessagePrivate *priv;
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	gpgme_sigsum_t status;
	GMimeStream *test_stream;
 	GMimeMultipartEncrypted *mpe;
	gpgme_decrypt_result_t result = NULL;
	gpgme_verify_result_t signatures = NULL;
	GMimeEntity *decrypted;
	GMimeMessage *message;
	Exception *ex = NULL;
	GMimeParser *parser;
	GByteArray *buf[2];
	GError *err = NULL;
	char *ret = NULL;
	
	g_mime_stream_reset (stream);
	g_mime_stream_reset (cleartext);
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	
	message = g_mime_parser_construct_message (parser, NULL, NULL);
	g_object_unref (parser);
	
	priv = _g_mime_message_get_private (message);

	if (!GMIME_IS_MULTIPART_ENCRYPTED (priv->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/encrypted?");
		g_object_unref (message);
		throw (ex);
	}
	
	mpe = (GMimeMultipartEncrypted *) priv->mime_part;
	
	/* okay, now to test our decrypt function... */
	g_assert_cmpuint (gpgme_set_ctx_flag (ctx, "export-session-key", "1"), ==, 0);
	g_assert_cmpuint (gpgme_set_ctx_flag (ctx, "override-session-key", session_key ? session_key : ""), ==, 0);
	decrypted = g_mime_multipart_encrypted_decrypt (mpe, ctx, &result, &signatures, &err);
	g_assert_cmpstr (gpgme_get_ctx_flag (ctx, "export-session-key"), ==, "1");
	g_assert_cmpstr (gpgme_get_ctx_flag (ctx, "override-session-key"), ==, session_key ? session_key : "");
	gpgme_set_ctx_flag (ctx, "override-session-key", "");
	gpgme_set_ctx_flag (ctx, "export-session-key", "0");
	g_object_unref (message);
	if (!decrypted || err != NULL) {
		g_clear_pointer (&result, gpgme_result_unref);
		g_clear_pointer (&signatures, gpgme_result_unref);
		g_clear_object (&decrypted);
		ex = exception_new ("decryption failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
#if GPGME_VERSION_NUMBER >= 0x010800
	if (!result->session_key) {
		ex = exception_new ("No session key returned!");
		if (signatures)
			gpgme_result_unref (signatures);
		gpgme_result_unref (result);
		g_object_unref (decrypted);
		throw (ex);
	}
	
	ret = g_strdup (result->session_key);
#endif
	
	if (signatures)
		v(print_verify_results (signatures));
	
	if (sign) {
		status = get_sig_status (signatures);
		
		if ((status & GPGME_SIGSUM_RED) != 0)
			ex = exception_new ("signature status expected to be GOOD");
	} else {
		if (signatures && signatures->signatures)
			ex = exception_new ("signature status expected to be NONE");
	}
	
	if (signatures)
		gpgme_result_unref (signatures);
	gpgme_result_unref (result);
	
	if (ex != NULL) {
		g_free (ret);
		ret = NULL;
		g_object_unref (decrypted);
		throw (ex);
	}
	
	test_stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream (decrypted, format, test_stream);
	g_object_unref (decrypted);
	
	buf[0] = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) cleartext);
	buf[1] = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) test_stream);
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	
	g_object_unref (test_stream);
	
	if (ex != NULL) {
		g_free (ret);
		ret = NULL;
		throw (ex);
	}
	
	return ret;
}

static void
import_key (gpgme_ctx_t ctx, const char *path)
{
	GMimeStream *stream;
	GError *err = NULL;
	Exception *ex;
	int fd;
	
	if ((fd = open (path, O_RDONLY, 0)) == -1)
		throw (exception_new ("open() failed: %s", g_strerror (errno)));
	
	stream = g_mime_stream_fs_new (fd);
	test_crypto_import (ctx, stream, &err);
	g_object_unref (stream);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}

static GMimePart *
create_mime_part (void)
{
	GMimeTextPart *part;
	
	part = g_mime_text_part_new_with_subtype ("plain");
	g_mime_text_part_set_text (part, "This is the body of the message...\n\n"
				   "Does inline-PGP support work properly?\n\n"
				   "Let's find out!\n\n");
	g_mime_part_set_content_encoding ((GMimePart *) part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	g_mime_text_part_set_charset (part, "UTF-8");
	
	return (GMimePart *) part;
}

static void
test_openpgp_sign (gpgme_ctx_t ctx)
{
	GMimeContentPrivate *priv_priv;
	GMimeContentPrivate *content_priv;
	GMimePartPrivate *priv;
	gpgme_verify_result_t signatures = NULL;
	GMimeStream *original;
	GMimePart *mime_part;
	Exception *ex = NULL;
	GError *err = NULL;
	GByteArray *buf[2];
	
	mime_part = create_mime_part ();
	priv = _g_mime_part_get_private (mime_part);

	priv_priv = _g_mime_content_get_private (priv->content);

	original = priv_priv->stream;
	g_object_ref (original);
	
	if (!g_mime_part_openpgp_sign (mime_part, ctx, NULL, &err)) {
		ex = exception_new ("signing failed: %s", err->message);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_SIGNED) {
		g_object_unref (mime_part);
		g_object_unref (original);
		
		throw (exception_new ("OpenPGP data property not updated after signing"));
	}
	
	if (!g_mime_part_openpgp_verify (mime_part, ctx, &signatures, &err)) {
		g_clear_pointer (&signatures, gpgme_result_unref);
		ex = exception_new ("verifying failed: %s", err->message);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_NONE) {
		gpgme_result_unref (signatures);
		g_object_unref (mime_part);
		g_object_unref (original);
		
		throw (exception_new ("OpenPGP data property not updated after verifying"));
	}

	buf[0] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (original));
	content_priv = _g_mime_content_get_private (priv->content);

	buf[1] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (content_priv->stream));
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("extracted data does not match original cleartext");
	if (ex == NULL && (get_sig_status (signatures) & GPGME_SIGSUM_RED))
		ex = exception_new ("inline signature is not valid");
	
	gpgme_result_unref (signatures);
	g_object_unref (mime_part);
	g_object_unref (original);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_openpgp_encrypt (gpgme_ctx_t ctx, gboolean sign)
{
	GMimeContentPrivate *priv_priv;
	GMimeContentPrivate *content_priv;
	GMimePartPrivate *priv;
	gpgme_decrypt_result_t result = NULL;
	gpgme_verify_result_t signatures = NULL;
	GMimeStream *original;
	GMimePart *mime_part;
	Exception *ex = NULL;
	GError *err = NULL;
	GByteArray *buf[2];
	gpgme_key_t rcpts[2] = { NULL, NULL };
	
	rcpts[0] = test_crypto_key (ctx, "no.user@no.domain", FALSE);
	
	mime_part = create_mime_part ();
	priv = _g_mime_part_get_private (mime_part);

	priv_priv = _g_mime_content_get_private (priv->content);

	original = priv_priv->stream;
	g_object_ref (original);
	
	if (!g_mime_part_openpgp_encrypt (mime_part, ctx, sign, GPGME_ENCRYPT_ALWAYS_TRUST, rcpts, NULL, NULL, &err)) {
		ex = exception_new ("encrypting failed: %s", err->message);
		gpgme_key_unref (rcpts[0]);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	gpgme_key_unref (rcpts[0]);
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_ENCRYPTED) {
		g_object_unref (mime_part);
		g_object_unref (original);
		
		throw (exception_new ("OpenPGP data property not updated after encrypting"));
	}
	
	if (!g_mime_part_openpgp_decrypt (mime_part, ctx, &result, &signatures, &err)) {
		g_clear_pointer (&result, gpgme_result_unref);
		g_clear_pointer (&signatures, gpgme_result_unref);
		ex = exception_new ("decrypting failed: %s", err->message);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_NONE) {
		g_object_unref (mime_part);
		g_object_unref (original);
		if (signatures)
			gpgme_result_unref (signatures);
		gpgme_result_unref (result);
		
		throw (exception_new ("OpenPGP data property not updated after decrypting"));
	}
	
	buf[0] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (original));
	content_priv = _g_mime_content_get_private (priv->content);

	buf[1] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (content_priv->stream));
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	if (ex == NULL && sign && (get_sig_status (signatures) & GPGME_SIGSUM_RED))
		ex = exception_new ("inline encrypted signature is not valid");
	if (ex == NULL && !sign && signatures && signatures->signatures)
		ex = exception_new ("unexpected inline encrypted signature");
	
	g_object_unref (mime_part);
	g_object_unref (original);
	if (signatures)
		gpgme_result_unref (signatures);
	gpgme_result_unref (result);
	
	if (ex != NULL)
		throw (ex);
}

static gpgme_ctx_t
create_gpg_context (void)
{
	gpgme_ctx_t ctx;
	
	ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);

	
	return ctx;
}

static void
assert_context (gpgme_ctx_t ctx, int armor, int textmode, const char *fingerprint)
{
	gpgme_passphrase_cb_t callback;
	gpgme_key_t signer;
	void *hook;

	g_assert_cmpint (gpgme_get_armor (ctx), ==, armor);
	g_assert_cmpint (gpgme_get_textmode (ctx), ==, textmode);
	g_assert_cmpint (gpgme_get_protocol (ctx), ==, GPGME_PROTOCOL_OpenPGP);
	g_assert_cmpint (gpgme_get_offline (ctx), ==, 1);
	g_assert_cmpint (gpgme_get_pinentry_mode (ctx), ==, GPGME_PINENTRY_MODE_LOOPBACK);
	gpgme_get_passphrase_cb (ctx, &callback, &hook);
	g_assert_true (callback == test_passphrase);
	g_assert_null (hook);
	g_assert_cmpstr (gpgme_get_ctx_flag (ctx, "export-session-key"), ==, "1");
	g_assert_cmpuint (gpgme_signers_count (ctx), ==, 1);
	signer = gpgme_signers_enum (ctx, 0);
	g_assert_cmpstr (signer->subkeys->fpr, ==, fingerprint);
	gpgme_key_unref (signer);
}

static void
test_context_settings (gpgme_ctx_t ctx)
{
	gpgme_key_t key = test_crypto_key (ctx, "no.user@no.domain", FALSE);
	gpgme_key_t recipients[] = { key, NULL };
	gpgme_verify_result_t verified = NULL;
	gpgme_sign_result_t signed_result = NULL;
	gpgme_decrypt_result_t decrypted_result = NULL;
	GMimeMultipartSigned *signed_part;
	GMimeMultipartEncrypted *encrypted;
	GMimePart *part;
	GMimeEntity *decrypted;
	GError *error = NULL;
	int armor, textmode;

	g_assert_cmpuint (gpgme_set_ctx_flag (ctx, "export-session-key", "1"), ==, 0);
	for (armor = 0; armor < 2; armor++) {
		for (textmode = 0; textmode < 2; textmode++) {
			GMimeContent *saved_content;

			gpgme_set_armor (ctx, armor);
			gpgme_set_textmode (ctx, textmode);
			part = create_mime_part ();
			signed_part = g_mime_multipart_signed_sign (ctx, GMIME_ENTITY (part), &signed_result, &error);
			g_assert_no_error (error);
			g_assert_nonnull (signed_part);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_multipart_signed_verify (signed_part, ctx, &verified, &error));
			g_assert_no_error (error);
			g_assert_false (get_sig_status (verified) & GPGME_SIGSUM_RED);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			gpgme_result_unref (verified);
			gpgme_result_unref (signed_result);
			{
				GMimePart *signature = GMIME_PART (g_mime_multipart_get_part (GMIME_MULTIPART (signed_part), 1));
				GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("invalid", 7);
				GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);

				g_mime_part_set_content (signature, content);
				g_object_unref (content);
				g_object_unref (stream);
				verified = NULL;
				g_assert_false (g_mime_multipart_signed_verify (signed_part, ctx, &verified, &error));
				g_assert_nonnull (error);
				g_assert_true (get_sig_status (verified) & GPGME_SIGSUM_RED);
				g_clear_pointer (&verified, gpgme_result_unref);
				g_clear_error (&error);
				assert_context (ctx, armor, textmode, key->subkeys->fpr);
			}
			g_object_unref (signed_part);
			encrypted = g_mime_multipart_encrypted_encrypt (ctx, GMIME_ENTITY (part), TRUE,
								      GPGME_ENCRYPT_ALWAYS_TRUST, recipients, NULL, NULL, &error);
			g_assert_no_error (error);
			g_assert_nonnull (encrypted);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			decrypted = g_mime_multipart_encrypted_decrypt (encrypted, ctx, &decrypted_result, &verified, &error);
			g_assert_no_error (error);
			g_assert_nonnull (decrypted);
			g_assert_nonnull (decrypted_result->session_key);
			g_assert_false (get_sig_status (verified) & GPGME_SIGSUM_RED);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			gpgme_result_unref (decrypted_result);
			gpgme_result_unref (verified);
			g_object_unref (decrypted);
			{
				GMimePart *ciphertext = GMIME_PART (g_mime_multipart_get_part (GMIME_MULTIPART (encrypted), 1));
				GMimeStream *stream = g_mime_stream_mem_new_with_buffer ("invalid", 7);
				GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);

				g_mime_part_set_content (ciphertext, content);
				g_object_unref (content);
				g_object_unref (stream);
				decrypted_result = NULL;
				verified = NULL;
				g_assert_null (g_mime_multipart_encrypted_decrypt (encrypted, ctx, &decrypted_result, &verified, &error));
				g_assert_nonnull (error);
				g_clear_pointer (&decrypted_result, gpgme_result_unref);
				g_clear_pointer (&verified, gpgme_result_unref);
				g_clear_error (&error);
				assert_context (ctx, armor, textmode, key->subkeys->fpr);
			}
			g_object_unref (encrypted);
			g_assert_true (g_mime_part_openpgp_sign (part, ctx, NULL, &error));
			g_assert_no_error (error);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_part_openpgp_verify (part, ctx, &verified, &error));
			g_assert_no_error (error);
			g_assert_false (get_sig_status (verified) & GPGME_SIGSUM_RED);
			gpgme_result_unref (verified);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_part_openpgp_encrypt (part, ctx, FALSE, GPGME_ENCRYPT_ALWAYS_TRUST,
								 recipients, NULL, NULL, &error));
			g_assert_no_error (error);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_part_openpgp_decrypt (part, ctx, NULL, NULL, &error));
			g_assert_no_error (error);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);

			/* Backend failure after MIME/inline settings have been applied. */
			saved_content = g_object_ref (g_mime_part_get_content (part));
			verified = (gpgme_verify_result_t) GINT_TO_POINTER (1);
			g_assert_false (g_mime_part_openpgp_verify (part, ctx, &verified, &error));
			g_assert_nonnull (error);
			g_assert_true (get_sig_status (verified) & GPGME_SIGSUM_RED);
			g_clear_pointer (&verified, gpgme_result_unref);
			g_clear_error (&error);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_part_get_content (part) == saved_content);
			g_assert_cmpint (g_mime_part_get_openpgp_data (part), ==, GMIME_OPENPGP_DATA_NONE);
			decrypted_result = (gpgme_decrypt_result_t) GINT_TO_POINTER (1);
			g_assert_cmpuint (gpgme_set_ctx_flag (ctx, "override-session-key",
				"9:0000000000000000000000000000000000000000000000000000000000000000"), ==, 0);
			g_assert_false (g_mime_part_openpgp_decrypt (part, ctx, &decrypted_result, &verified, &error));
			g_assert_nonnull (error);
			g_clear_pointer (&decrypted_result, gpgme_result_unref);
			g_assert_true (get_sig_status (verified) & GPGME_SIGSUM_RED);
			g_clear_pointer (&verified, gpgme_result_unref);
			g_clear_error (&error);
			g_assert_cmpstr (gpgme_get_ctx_flag (ctx, "override-session-key"), ==,
				"9:0000000000000000000000000000000000000000000000000000000000000000");
			g_assert_cmpuint (gpgme_set_ctx_flag (ctx, "override-session-key", ""), ==, 0);
			assert_context (ctx, armor, textmode, key->subkeys->fpr);
			g_assert_true (g_mime_part_get_content (part) == saved_content);
			g_assert_cmpint (g_mime_part_get_openpgp_data (part), ==, GMIME_OPENPGP_DATA_NONE);
			g_object_unref (saved_content);
			g_object_unref (part);
		}
	}
	gpgme_set_ctx_flag (ctx, "export-session-key", "0");
	gpgme_set_armor (ctx, 0);
	gpgme_set_textmode (ctx, 0);
	gpgme_key_unref (key);
}

static void
test_crypto_errors (gpgme_ctx_t ctx)
{
	gpgme_ctx_t cms = test_crypto_context (GPGME_PROTOCOL_CMS);
	GMimeMultipartSigned *signed_part = g_mime_multipart_signed_new ();
	GMimeMultipartEncrypted *encrypted = g_mime_multipart_encrypted_new ();
	GMimeApplicationPkcs7Mime *pkcs7 = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_SIGNED_DATA);
	GMimePart *part = g_mime_part_new ();
	gpgme_verify_result_t verified = NULL;
	gpgme_decrypt_result_t decrypted = NULL;
	gpgme_sign_result_t signed_result = NULL;
	GMimeEntity *entity = NULL;
	GError *error = NULL;

	g_assert_false (g_mime_part_openpgp_sign (part, cms, &signed_result, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_assert_null (signed_result);
	g_clear_error (&error);
	g_assert_null (g_mime_multipart_encrypted_encrypt (cms, GMIME_ENTITY (part), FALSE,
							 0, NULL, NULL, NULL, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_clear_error (&error);
	g_assert_null (g_mime_application_pkcs7_mime_sign (ctx, GMIME_ENTITY (part), &signed_result, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_clear_error (&error);
	g_assert_false (g_mime_application_pkcs7_mime_verify (pkcs7, ctx, &entity, &verified, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR);
	g_assert_null (entity);
	g_assert_null (verified);
	g_clear_error (&error);

	g_assert_false (g_mime_part_openpgp_sign (part, ctx, &signed_result, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_false (g_mime_part_openpgp_verify (part, ctx, &verified, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_false (g_mime_part_openpgp_decrypt (part, ctx, &decrypted, &verified, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_false (g_mime_multipart_signed_verify (signed_part, ctx, &verified, &error));
	g_assert_nonnull (error);
	g_assert_null (verified);
	g_clear_error (&error);
	g_assert_null (g_mime_multipart_encrypted_decrypt (encrypted, ctx, &decrypted, &verified, &error));
	g_assert_nonnull (error);
	g_assert_null (decrypted);
	g_assert_null (verified);
	g_clear_error (&error);
	g_assert_false (g_mime_application_pkcs7_mime_verify (pkcs7, cms, &entity, &verified, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_assert_null (entity);
	g_assert_null (verified);
	g_clear_error (&error);
	g_assert_null (g_mime_application_pkcs7_mime_decrypt (pkcs7, cms, &decrypted, &error));
	g_assert_nonnull (error);
	g_assert_null (decrypted);
	g_clear_error (&error);
	g_object_unref (part);
	g_object_unref (pkcs7);
	g_object_unref (encrypted);
	g_object_unref (signed_part);
	gpgme_release (cms);
}

#endif

int main (int argc, char *argv[])
{
#ifdef ENABLE_CRYPTO
	GMimeStream *stream = NULL, *cleartext = NULL;
	const char *datadir = "data/pgpmime";
	char *session_key = NULL;
	gpgme_ctx_t ctx;
	char *gpg, *key;
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (!(gpg = g_find_program_in_path ("gpg2")))
		if (!(gpg = g_find_program_in_path ("gpg")))
			return EXIT_FAILURE;
	
	if (testsuite_setup_gpghome (gpg, "pgpmime") != 0)
		return EXIT_FAILURE;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("PGP/MIME implementation");



	ctx = create_gpg_context ();
	
	testsuite_check ("GPGME OpenPGP::import");
	try {
		key = g_build_filename (datadir, "gmime.gpg.pub", NULL);
		import_key (ctx, key);
		g_free (key);
		
		key = g_build_filename (datadir, "gmime.gpg.sec", NULL);
		import_key (ctx, key);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME OpenPGP::import failed: %s", ex->message);
		return EXIT_FAILURE;
	} finally;
	
	test_crypto_signer (ctx, "no.user@no.domain");

	testsuite_check ("Native context settings and signer preservation on success and failure");
	test_context_settings (ctx);
	testsuite_check_passed ();
	testsuite_check ("Wrong protocols, missing content and cleared failure outputs");
	test_crypto_errors (ctx);
	testsuite_check_passed ();

	testsuite_check ("multipart/signed");
	try {
		test_multipart_signed (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/signed failed: %s", ex->message);
	} finally;
	
	testsuite_check ("multipart/encrypted");
	try {
		create_encrypted_message (ctx, FALSE, &cleartext, &stream);
		session_key = test_multipart_encrypted (ctx, FALSE, cleartext, stream, NULL);
#if GPGME_VERSION_NUMBER >= 0x010800
		if (testsuite_can_safely_override_session_key (gpg))
			g_free (test_multipart_encrypted (ctx, FALSE, cleartext, stream, session_key));
#endif
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted failed: %s", ex->message);
	} finally;
	
	if (cleartext) {
		g_object_unref (cleartext);
		cleartext = NULL;
	}
	
	if (stream) {
		g_object_unref (stream);
		stream = NULL;
	}
	
	g_free (session_key);
	
	testsuite_check ("multipart/encrypted+sign");
	try {
		create_encrypted_message (ctx, TRUE, &cleartext, &stream);
		session_key = test_multipart_encrypted (ctx, TRUE, cleartext, stream, NULL);
#if GPGME_VERSION_NUMBER >= 0x010800
		if (testsuite_can_safely_override_session_key (gpg))
			g_free (test_multipart_encrypted (ctx, TRUE, cleartext, stream, session_key));
#endif
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted+sign failed: %s", ex->message);
	} finally;
	
	if (cleartext) {
		g_object_unref (cleartext);
		cleartext = NULL;
	}
	
	if (stream) {
		g_object_unref (stream);
		stream = NULL;
	}
	
	g_free (session_key);
	
	testsuite_check ("rfc4880 sign");
	try {
		test_openpgp_sign (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 sign failed: %s", ex->message);
	} finally;
	
	testsuite_check ("rfc4880 encrypt");
	try {
		test_openpgp_encrypt (ctx, FALSE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 encrypt failed: %s", ex->message);
	} finally;
	
	testsuite_check ("rfc4880 sign+encrypt");
	try {
		test_openpgp_encrypt (ctx, TRUE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 sign+encrypt failed: %s", ex->message);
	} finally;
	
	gpgme_release (ctx);
	g_free (gpg);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
#else
	fprintf (stderr, "PGP support not enabled in this build.\n");
	return EXIT_SUCCESS;
#endif
}
