/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gpgme.h>
#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define v(x) if (verbose > 3) x

static gboolean
request_passwd (GMimeCryptoContext *ctx, const char *user_id, const char *prompt, gboolean reprompt, GMimeStream *response, GError **err)
{
	g_mime_stream_write_string (response, "no.secret\n");
	
	return TRUE;
}

static GMimeSignatureStatus
get_sig_status (GMimeSignatureList *signatures)
{
	GMimeSignatureStatus status = 0;
	GMimeSignature *sig;
	int i;
	
	if (!signatures || signatures->array->len == 0)
		return GMIME_SIGNATURE_STATUS_RED;
	
	for (i = 0; i < g_mime_signature_list_length (signatures); i++) {
		sig = g_mime_signature_list_get_signature (signatures, i);
		status |= g_mime_signature_get_status (sig);
	}
	
	return status;
}

static void
print_verify_results (GMimeSignatureList *signatures)
{
	GMimeSignatureStatus status;
	GMimeSignature *sig;
	int count, i;
	
	status = get_sig_status (signatures);
	
	fprintf (stdout, "Overall status: (0x%x) ", status);
	if ((status & GMIME_SIGNATURE_STATUS_RED) != 0)
		fputs ("BAD\n", stdout);
	else if ((status & GMIME_SIGNATURE_STATUS_GREEN) != 0)
		fputs ("GOOD\n", stdout);
	else if ((status & GMIME_SIGNATURE_STATUS_VALID) != 0)
		fputs ("VALID\n", stdout);
	else
		fputs ("UNKNOWN\n", stdout);
	
	fputs ("\nSignatures:\n", stdout);
	count = g_mime_signature_list_length (signatures);
	for (i = 0; i < count; i++) {
		sig = g_mime_signature_list_get_signature (signatures, i);
		
		fprintf (stdout, "\tName: %s\n", sig->cert->name ? sig->cert->name : "(null)");
		fprintf (stdout, "\tKeyId: %s\n", sig->cert->keyid ? sig->cert->keyid : "(null)");
		fprintf (stdout, "\tUserID: %s\n", sig->cert->user_id ? sig->cert->user_id : "(null)");
		fprintf (stdout, "\tFingerprint: %s\n", sig->cert->fingerprint ? sig->cert->fingerprint : "(null)");
		fprintf (stdout, "\tTrust: ");
		
		switch (sig->cert->trust) {
		case GMIME_TRUST_UNKNOWN:
			fputs ("Unknown\n", stdout);
			break;
		case GMIME_TRUST_NEVER:
			fputs ("Never\n", stdout);
			break;
		case GMIME_TRUST_UNDEFINED:
			fputs ("Undefined\n", stdout);
			break;
		case GMIME_TRUST_MARGINAL:
			fputs ("Marginal\n", stdout);
			break;
		case GMIME_TRUST_FULL:
			fputs ("Full\n", stdout);
			break;
		case GMIME_TRUST_ULTIMATE:
			fputs ("Ultimate\n", stdout);
			break;
		}
		
		fprintf (stdout, "\tStatus: ");
		if ((sig->status & GMIME_SIGNATURE_STATUS_RED) != 0)
			fputs ("BAD\n", stdout);
		else if ((sig->status & GMIME_SIGNATURE_STATUS_GREEN) != 0)
			fputs ("GOOD\n", stdout);
		else if ((sig->status & GMIME_SIGNATURE_STATUS_VALID) != 0)
			fputs ("VALID\n", stdout);
		else
			fputs ("UNKNOWN\n", stdout);
		
		fprintf (stdout, "\tSignature made on %s", ctime (&sig->created));
		if (sig->expires != (time_t) 0)
			fprintf (stdout, "\tSignature expires on %s", ctime (&sig->expires));
		else
			fprintf (stdout, "\tSignature never expires\n");
		
		fprintf (stdout, "\tErrors: ");
		if (sig->status & GMIME_SIGNATURE_STATUS_KEY_REVOKED)
			fputs ("Key Revoked, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_KEY_EXPIRED)
			fputs ("Key Expired, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_SIG_EXPIRED)
			fputs ("Sig Expired, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_KEY_MISSING)
			fputs ("Key Missing, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_CRL_MISSING)
			fputs ("CRL Missing, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_CRL_TOO_OLD)
			fputs ("CRL Too Old, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_BAD_POLICY)
			fputs ("Bad Policy, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_SYS_ERROR)
			fputs ("System Error, ", stdout);
		if (sig->status & GMIME_SIGNATURE_STATUS_TOFU_CONFLICT)
			fputs ("Tofu Conflict", stdout);
		if ((sig->status & GMIME_SIGNATURE_STATUS_ERROR_MASK) == 0)
			fputs ("None", stdout);
		fputc ('\n', stdout);
		
		if (i + 1 < count)
			fputc ('\n', stdout);
	}
}

static GMimeMessage *
create_message (GMimeObject *body)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	InternetAddressList *list;
	InternetAddress *mailbox;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	
	message = g_mime_message_new (TRUE);
	
	mailbox = internet_address_mailbox_new ("Jeffrey Stedfast", "fejj@helixcode.com");
	list = g_mime_message_get_from (message);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = internet_address_mailbox_new ("Jeffrey Stedfast", "fejj@helixcode.com");
	list = g_mime_message_get_reply_to (message);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = internet_address_mailbox_new ("Federico Mena-Quintero", "federico@helixcode.com");
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	g_mime_message_set_subject (message, "This is a test message", NULL);
	g_mime_object_set_header ((GMimeObject *) message, "X-Mailer", "main.c", NULL);
	g_mime_message_set_mime_part (message, body);
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) message, format, stream);
	g_mime_stream_reset (stream);
	g_object_unref (message);
	
	/*fprintf (stderr, "-----BEGIN RAW MESSAGE-----\n%.*s-----END RAW MESSAGE-----\n",
		 GMIME_STREAM_MEM (stream)->buffer->len,
		 GMIME_STREAM_MEM (stream)->buffer->data);*/
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser, NULL);
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
test_multipart_signed (GMimeCryptoContext *ctx)
{
	GMimeSignatureList *signatures;
	GMimeSignatureStatus status;
	GMimeMultipartSigned *mps;
	GMimeMessage *message;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	
	part = g_mime_text_part_new_with_subtype ("plain");
	g_mime_text_part_set_text (part, MULTIPART_SIGNED_CONTENT);
	
	/* sign the part */
	mps = g_mime_multipart_signed_sign (ctx, (GMimeObject *) part, "no.user@no.domain", &err);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("signing failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeObject *) mps);
	g_object_unref (mps);
	
	if (!GMIME_IS_MULTIPART_SIGNED (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/signed?");
		g_object_unref (message);
		throw (ex);
	}
	
	mps = (GMimeMultipartSigned *) message->mime_part;
	
	if (!(signatures = g_mime_multipart_signed_verify (mps, 0, &err))) {
		ex = exception_new ("%s", err->message);
		v(fputs ("failed.\n", stdout));
		g_error_free (err);
		throw (ex);
	}
	
	v(print_verify_results (signatures));
	
	status = get_sig_status (signatures);
	g_object_unref (signatures);
	g_object_unref (message);
	
	if (status & GMIME_SIGNATURE_STATUS_RED)
		throw (exception_new ("signature status was BAD"));
}

#define MULTIPART_ENCRYPTED_CONTENT "This is a test of multipart/encrypted.\n"

static void
create_encrypted_message (GMimeCryptoContext *ctx, gboolean sign,
			  GMimeStream **cleartext_out, GMimeStream **stream_out)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeStream *cleartext, *stream;
	GMimeMultipartEncrypted *mpe;
	InternetAddressList *list;
	InternetAddress *mailbox;
	GPtrArray *recipients;
	GMimeMessage *message;
	Exception *ex = NULL;
	GError *err = NULL;
	GMimeTextPart *part;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, MULTIPART_ENCRYPTED_CONTENT);
	
	/* hold onto this for comparison later */
	cleartext = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) part, format, cleartext);
	g_mime_stream_reset (cleartext);
	
	/* encrypt the part */
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	mpe = g_mime_multipart_encrypted_encrypt (ctx, (GMimeObject *) part, sign, "no.user@no.domain",
						  GMIME_ENCRYPT_ALWAYS_TRUST, recipients, &err);
	g_ptr_array_free (recipients, TRUE);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("encryption failed: %s", err->message);
		g_object_unref (cleartext);
		g_error_free (err);
		throw (ex);
	}
	
	message = g_mime_message_new (TRUE);
	
	mailbox = internet_address_mailbox_new ("Jeffrey Stedfast", "fejj@helixcode.com");
	list = g_mime_message_get_from (message);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = internet_address_mailbox_new ("Jeffrey Stedfast", "fejj@helixcode.com");
	list = g_mime_message_get_reply_to (message);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	mailbox = internet_address_mailbox_new ("Federico Mena-Quintero", "federico@helixcode.com");
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	internet_address_list_add (list, mailbox);
	g_object_unref (mailbox);
	
	g_mime_message_set_subject (message, "This is a test message", NULL);
	g_mime_object_set_header ((GMimeObject *) message, "X-Mailer", "main.c", NULL);
	g_mime_message_set_mime_part (message, (GMimeObject *) mpe);
	g_object_unref (mpe);
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) message, format, stream);
	g_object_unref (message);
	
	*stream_out = stream;
	*cleartext_out = cleartext;
}	

static char *
test_multipart_encrypted (GMimeCryptoContext *ctx, gboolean sign,
			  GMimeStream *cleartext, GMimeStream *stream,
			  const char *session_key)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeSignatureStatus status;
	GMimeStream *test_stream;
 	GMimeMultipartEncrypted *mpe;
	GMimeDecryptResult *result;
	GMimeObject *decrypted;
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
	
	message = g_mime_parser_construct_message (parser, NULL);
	g_object_unref (parser);
	
	if (!GMIME_IS_MULTIPART_ENCRYPTED (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/encrypted?");
		g_object_unref (message);
		throw (ex);
	}
	
	mpe = (GMimeMultipartEncrypted *) message->mime_part;
	
	/* okay, now to test our decrypt function... */
	decrypted = g_mime_multipart_encrypted_decrypt (mpe, GMIME_DECRYPT_EXPORT_SESSION_KEY, session_key, &result, &err);
	if (!decrypted || err != NULL) {
		ex = exception_new ("decryption failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
#if GPGME_VERSION_NUMBER >= 0x010800
	if (!result->session_key) {
		ex = exception_new ("No session key returned!");
		throw (ex);
	}
	
	ret = g_strdup (result->session_key);
#endif
	
	if (result->signatures)
		v(print_verify_results (result->signatures));
	
	if (sign) {
		status = get_sig_status (result->signatures);
		
		if ((status & GMIME_SIGNATURE_STATUS_RED) != 0)
			ex = exception_new ("signature status expected to be GOOD");
	} else {
		if (result->signatures)
			ex = exception_new ("signature status expected to be NONE");
	}
	
	g_object_unref (result);
	
	if (ex != NULL) {
		g_free (ret);
		ret = NULL;
		throw (ex);
	}
	
	test_stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (decrypted, format, test_stream);
	
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
import_key (GMimeCryptoContext *ctx, const char *path)
{
	GMimeStream *stream;
	GError *err = NULL;
	Exception *ex;
	int fd;
	
	if ((fd = open (path, O_RDONLY, 0)) == -1)
		throw (exception_new ("open() failed: %s", g_strerror (errno)));
	
	stream = g_mime_stream_fs_new (fd);
	g_mime_crypto_context_import_keys (ctx, stream, &err);
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
test_openpgp_sign (void)
{
	GMimeSignatureList *signatures;
	GMimeStream *original;
	GMimePart *mime_part;
	Exception *ex = NULL;
	GError *err = NULL;
	GByteArray *buf[2];
	
	mime_part = create_mime_part ();
	original = mime_part->content->stream;
	g_object_ref (original);
	
	if (!g_mime_part_openpgp_sign (mime_part, "no.user@no.domain", &err)) {
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
	
	if (!(signatures = g_mime_part_openpgp_verify (mime_part, 0, &err))) {
		ex = exception_new ("verifying failed: %s", err->message);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_NONE) {
		g_object_unref (signatures);
		g_object_unref (mime_part);
		g_object_unref (original);
		
		throw (exception_new ("OpenPGP data property not updated after verifying"));
	}

	buf[0] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (original));
	buf[1] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (mime_part->content->stream));
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("extracted data does not match original cleartext");
	
	g_object_unref (signatures);
	g_object_unref (mime_part);
	g_object_unref (original);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_openpgp_encrypt (gboolean sign)
{
	GMimeDecryptResult *result;
	GMimeStream *original;
	GMimePart *mime_part;
	Exception *ex = NULL;
	GError *err = NULL;
	GByteArray *buf[2];
	GPtrArray *rcpts;
	
	rcpts = g_ptr_array_new ();
	g_ptr_array_add (rcpts, "no.user@no.domain");
	
	mime_part = create_mime_part ();
	original = mime_part->content->stream;
	g_object_ref (original);
	
	if (!g_mime_part_openpgp_encrypt (mime_part, sign, "no.user@no.domain", GMIME_ENCRYPT_ALWAYS_TRUST, rcpts, &err)) {
		ex = exception_new ("encrypting failed: %s", err->message);
		g_ptr_array_free (rcpts, TRUE);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	g_ptr_array_free (rcpts, TRUE);
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_ENCRYPTED) {
		g_object_unref (mime_part);
		g_object_unref (original);
		
		throw (exception_new ("OpenPGP data property not updated after encrypting"));
	}
	
	if (!(result = g_mime_part_openpgp_decrypt (mime_part, 0, NULL, &err))) {
		ex = exception_new ("decrypting failed: %s", err->message);
		g_object_unref (mime_part);
		g_object_unref (original);
		g_error_free (err);
		throw (ex);
	}
	
	if (g_mime_part_get_openpgp_data (mime_part) != GMIME_OPENPGP_DATA_NONE) {
		g_object_unref (mime_part);
		g_object_unref (original);
		g_object_unref (result);
		
		throw (exception_new ("OpenPGP data property not updated after decrypting"));
	}
	
	buf[0] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (original));
	buf[1] = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (mime_part->content->stream));
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	
	g_object_unref (mime_part);
	g_object_unref (original);
	g_object_unref (result);
	
	if (ex != NULL)
		throw (ex);
}

static GMimeCryptoContext *
create_gpg_context (void)
{
	GMimeCryptoContext *ctx;
	
	ctx = g_mime_gpg_context_new ();
	g_mime_crypto_context_set_request_password (ctx, request_passwd);
	
	return ctx;
}

int main (int argc, char *argv[])
{
#ifdef ENABLE_CRYPTO
	GMimeStream *stream = NULL, *cleartext = NULL;
	const char *datadir = "data/pgpmime";
	char *session_key = NULL;
	GMimeCryptoContext *ctx;
	char *gpg, *key;
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (!(gpg = g_find_program_in_path ("gpg2")))
		if (!(gpg = g_find_program_in_path ("gpg")))
			return EXIT_FAILURE;
	
	if (testsuite_setup_gpghome (gpg) != 0)
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
	
	g_mime_crypto_context_register ("application/pgp-encrypted", create_gpg_context);
	g_mime_crypto_context_register ("application/pgp-signature", create_gpg_context);
	g_mime_crypto_context_register ("application/pgp-keys", create_gpg_context);
	ctx = create_gpg_context ();
	
	testsuite_check ("GMimeGpgContext::import");
	try {
		key = g_build_filename (datadir, "gmime.gpg.pub", NULL);
		import_key (ctx, key);
		g_free (key);
		
		key = g_build_filename (datadir, "gmime.gpg.sec", NULL);
		import_key (ctx, key);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeGpgContext::import failed: %s", ex->message);
		return EXIT_FAILURE;
	} finally;
	
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
		test_openpgp_sign ();
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 sign failed: %s", ex->message);
	} finally;
	
	testsuite_check ("rfc4880 encrypt");
	try {
		test_openpgp_encrypt (FALSE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 encrypt failed: %s", ex->message);
	} finally;
	
	testsuite_check ("rfc4880 sign+encrypt");
	try {
		test_openpgp_encrypt (TRUE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("rfc4880 sign+encrypt failed: %s", ex->message);
	} finally;
	
	g_object_unref (ctx);
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
