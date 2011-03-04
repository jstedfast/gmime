/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2010 Jeffrey Stedfast
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

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define v(x) if (verbose > 3) x

static gboolean
request_passwd (GMimeCryptoContext *ctx, const char *user_id, const char *prompt_ctx, gboolean reprompt, GMimeStream *response, GError **err)
{
	g_mime_stream_write_string (response, "no.secret\n");
	
	return TRUE;
}

static GMimeSignerStatus
get_sig_status (GMimeSigner *signers)
{
	GMimeSignerStatus status = GMIME_SIGNER_STATUS_GOOD;
	GMimeSigner *signer = signers;
	
	if (signers == NULL)
		return GMIME_SIGNER_STATUS_ERROR;
	
	while (signer != NULL) {
		status = MAX (status, signer->status);
		signer = signer->next;
	}
	
	return status;
}

static void
print_verify_results (const GMimeSignatureValidity *validity)
{
	GMimeSigner *signer;
	
	switch (get_sig_status (validity->signers)) {
	case GMIME_SIGNER_STATUS_GOOD:
		fputs ("GOOD\n", stdout);
		break;
	case GMIME_SIGNER_STATUS_BAD:
		fputs ("BAD\n", stdout);
		break;
	case GMIME_SIGNER_STATUS_ERROR:
		fputs ("ERROR status\n", stdout);
		break;
	default:
		fputs ("Unknown enum value\n", stdout);
		break;
	}
	
	fputs ("\nSigners:\n", stdout);
	signer = validity->signers;
	while (signer != NULL) {
		fprintf (stdout, "\tName: %s\n", signer->name ? signer->name : "(null)");
		fprintf (stdout, "\tKeyId: %s\n", signer->keyid ? signer->keyid : "(null)");
		fprintf (stdout, "\tFingerprint: %s\n", signer->fingerprint ? signer->fingerprint : "(null)");
		fprintf (stdout, "\tTrust: ");
		
		switch (signer->trust) {
		case GMIME_SIGNER_TRUST_NONE:
			fputs ("None\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_NEVER:
			fputs ("Never\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_UNDEFINED:
			fputs ("Undefined\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_MARGINAL:
			fputs ("Marginal\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_FULLY:
			fputs ("Fully\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_ULTIMATE:
			fputs ("Ultimate\n", stdout);
			break;
		}
		
		fprintf (stdout, "\tStatus: ");
		switch (signer->status) {
		case GMIME_SIGNER_STATUS_GOOD:
			fputs ("GOOD\n", stdout);
			break;
		case GMIME_SIGNER_STATUS_BAD:
			fputs ("BAD\n", stdout);
			break;
		case GMIME_SIGNER_STATUS_ERROR:
			fputs ("ERROR\n", stdout);
			break;
		}
		
		fprintf (stdout, "\tSignature made on %s", ctime (&signer->sig_created));
		if (signer->sig_expires != (time_t) 0)
			fprintf (stdout, "\tSignature expires on %s", ctime (&signer->sig_expires));
		else
			fprintf (stdout, "\tSignature never expires\n");
		
		if (signer->errors) {
			fprintf (stdout, "\tErrors: ");
			if (signer->errors & GMIME_SIGNER_ERROR_EXPSIG)
				fputs ("Expired, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_NO_PUBKEY)
				fputs ("No Pub Key, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_EXPKEYSIG)
				fputs ("Key Expired, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_REVKEYSIG)
				fputs ("Key Revoked", stdout);
			fputc ('\n', stdout);
		} else {
			fprintf (stdout, "\tNo errors for this signer\n");
		}
		
		if ((signer = signer->next))
			fputc ('\n', stdout);
	}
	
	fprintf (stdout, "\nValidity diagnostics: \n%s\n",
		 g_mime_signature_validity_get_details (validity));
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
	GMimeSignatureValidity *validity;
	GMimeMultipartSigned *mps;
	GMimeDataWrapper *content;
	GMimeMessage *message;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *err = NULL;
	GMimePart *part;
	Exception *ex;
	
	part = g_mime_part_new_with_type ("text", "plain");
	
	stream = g_mime_stream_mem_new ();
	g_mime_stream_write_string (stream, MULTIPART_SIGNED_CONTENT);
#if 0
	"This is a test of the emergency broadcast system with an sha1 detach-sign.\n\n"
		"From now on, there will be text to try and break     \t  \n"
		"various things. For example, the F in \"From\" in the previous line...\n"
		"...and the first dot of this line have been pre-encoded in the QP encoding "
		"in order to test that GMime properly treats MIME part content as opaque.\n"
		"If this still verifies okay, then we have ourselves a winner I guess...\n";
#endif
	
	g_mime_stream_reset (stream);
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
	g_object_unref (stream);
	
	g_mime_part_set_content_object (part, content);
	g_object_unref (content);
	
	/* create the multipart/signed container part */
	mps = g_mime_multipart_signed_new ();
	
	/* sign the part */
	g_mime_multipart_signed_sign (mps, GMIME_OBJECT (part), ctx, "no.user@no.domain",
				      GMIME_CRYPTO_HASH_SHA1, &err);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("signing failed: %s", err->message);
		g_object_unref (mps);
		g_error_free (err);
		throw (ex);
	}
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				      "Federico Mena-Quintero",
				      "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_object_set_header ((GMimeObject *) message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, GMIME_OBJECT (mps));
	g_object_unref (mps);
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) message, stream);
	g_mime_stream_reset (stream);
	g_object_unref (message);
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser);
	g_object_unref (parser);
	
	if (!GMIME_IS_MULTIPART_SIGNED (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/signed?");
		g_object_unref (message);
		throw (ex);
	}
	
	mps = (GMimeMultipartSigned *) message->mime_part;
	
	v(fputs ("Trying to verify signature... ", stdout));
	if (!(validity = g_mime_multipart_signed_verify (mps, ctx, &err))) {
		ex = exception_new ("%s", err->message);
		v(fputs ("failed.\n", stdout));
		g_error_free (err);
		throw (ex);
	}
	
	v(print_verify_results (validity));
	g_mime_signature_validity_free (validity);
	
	g_object_unref (message);
}

#define MULTIPART_ENCRYPTED_CONTENT "This is a test of multipart/encrypted.\n"

static void
test_multipart_encrypted (GMimeCryptoContext *ctx, gboolean sign)
{
	GMimeStream *cleartext, *stream;
	GMimeMultipartEncrypted *mpe;
	GMimeDecryptionResult *result;
	GMimeDataWrapper *content;
	GMimeObject *decrypted;
	GPtrArray *recipients;
	GMimeMessage *message;
	Exception *ex = NULL;
	GMimeParser *parser;
	GByteArray *buf[2];
	GError *err = NULL;
	GMimePart *part;
	
	cleartext = g_mime_stream_mem_new ();
	g_mime_stream_write_string (cleartext, MULTIPART_ENCRYPTED_CONTENT);
	g_mime_stream_reset (cleartext);
	
	content = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (content, cleartext);
	g_object_unref (cleartext);
	
	part = g_mime_part_new_with_type ("text", "plain");
	g_mime_part_set_content_object (part, content);
	g_object_unref (content);
	
	/* hold onto this for comparison later */
	cleartext = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) part, cleartext);
	g_mime_stream_reset (cleartext);
	
	/* create the multipart/encrypted container part */
	mpe = g_mime_multipart_encrypted_new ();
	
	/* encrypt the part */
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	g_mime_multipart_encrypted_encrypt (mpe, GMIME_OBJECT (part), ctx, sign,
					    "no.user@no.domain", GMIME_CRYPTO_HASH_SHA256,
					    recipients, &err);
	g_ptr_array_free (recipients, TRUE);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("encryption failed: %s", err->message);
		g_object_unref (cleartext);
		g_object_unref (mpe);
		g_error_free (err);
		throw (ex);
	}
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				      "Federico Mena-Quintero",
				      "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_object_set_header ((GMimeObject *) message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, GMIME_OBJECT (mpe));
	g_object_unref (mpe);
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream ((GMimeObject *) message, stream);
	g_mime_stream_reset (stream);
	g_object_unref (message);
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser);
	g_object_unref (parser);
	
	if (!GMIME_IS_MULTIPART_ENCRYPTED (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not a multipart/encrypted?");
		g_object_unref (message);
		throw (ex);
	}
	
	mpe = (GMimeMultipartEncrypted *) message->mime_part;
	
	/* okay, now to test our decrypt function... */
	decrypted = g_mime_multipart_encrypted_decrypt (mpe, ctx, &result, &err);
	if (!decrypted || err != NULL) {
		ex = exception_new ("decryption failed: %s", err->message);
		g_mime_decryption_result_free (result);
		g_object_unref (cleartext);
		g_error_free (err);
		throw (ex);
	}
	
	if (result->validity)
		v(print_verify_results (result->validity));
	
	if (sign) {
		if (!result->validity || get_sig_status (result->validity->signers) != GMIME_SIGNER_STATUS_GOOD)
			ex = exception_new ("signature status expected to be GOOD");
	} else {
		if (result->validity && result->validity->signers != NULL)
			ex = exception_new ("signature status expected to be NONE");
	}
	
	g_mime_decryption_result_free (result);
	
	if (ex != NULL) {
		g_object_unref (cleartext);
		throw (ex);
	}
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (decrypted, stream);
	
	buf[0] = GMIME_STREAM_MEM (cleartext)->buffer;
	buf[1] = GMIME_STREAM_MEM (stream)->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	
	g_object_unref (cleartext);
	g_object_unref (stream);
	
	if (ex != NULL)
		throw (ex);
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

int main (int argc, char *argv[])
{
	const char *datadir = "data/pgpmime";
	GMimeCryptoContext *ctx;
	struct stat st;
	char *key;
	int i;
	
	g_mime_init (0);
	
	testsuite_init (argc, argv);
	
	/* reset .gnupg config directory */
	system ("/bin/rm -rf ./tmp");
	system ("/bin/mkdir ./tmp");
	setenv ("GNUPGHOME", "./tmp/.gnupg", 1);
	system ("/usr/bin/gpg --list-keys > /dev/null 2>&1");
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return EXIT_FAILURE;
	
	testsuite_start ("PGP/MIME implementation");
	
	ctx = g_mime_gpg_context_new (request_passwd, "/usr/bin/gpg");
	g_mime_gpg_context_set_always_trust ((GMimeGpgContext *) ctx, TRUE);
	
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
		test_multipart_encrypted (ctx, FALSE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted failed: %s", ex->message);
	} finally;
	
	testsuite_check ("multipart/encrypted+sign");
	try {
		test_multipart_encrypted (ctx, TRUE);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted+sign failed: %s", ex->message);
	} finally;
	
	g_object_unref (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	system ("/bin/rm -rf ./tmp");
	
	return testsuite_exit ();
}
