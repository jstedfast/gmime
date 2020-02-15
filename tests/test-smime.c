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

#include <gmime/gmime.h>
#include <gmime/gmime-pkcs7-context.h>

#include "testsuite.h"

extern int verbose;

#define v(x) if (verbose > 3) x

#if 0
static gboolean
request_passwd (GMimeCryptoContext *ctx, const char *user_id, const char *prompt, gboolean reprompt, GMimeStream *response, GError **err)
{
	g_mime_stream_write_string (response, "no.secret\n");
	
	return TRUE;
}
#endif

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
	
	fputs ("Overall status: ", stdout);
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
	GMimeMultipartSigned *mps;
	GMimeMessage *message;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	
	part = g_mime_text_part_new_with_subtype ("plain");
	g_mime_text_part_set_text (part, MULTIPART_SIGNED_CONTENT);
	
	/* sign the part */
	mps = g_mime_multipart_signed_sign (ctx, (GMimeObject *) part, "mimekit@example.com", &err);
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
		g_error_free (err);
		throw (ex);
	}
	
	v(print_verify_results (signatures));
	g_object_unref (signatures);
	
	g_object_unref (message);
}

#define SIGNED_CONTENT "This is a test of application/pkcs7-mime; smime-type=signed-data.\n"

static void
test_pkcs7_mime_sign (void)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeSignatureList *signatures;
	GMimeMessage *message;
	GMimeObject *entity;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	char *text;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, SIGNED_CONTENT);
	
	pkcs7_mime = g_mime_application_pkcs7_mime_sign ((GMimeObject *) part, "mimekit@example.com", &err);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("sign failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeObject *) pkcs7_mime);
	g_object_unref (pkcs7_mime);
	
	if (!GMIME_IS_APPLICATION_PKCS7_MIME (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not an application/pkcs7-mime?");
		g_object_unref (message);
		throw (ex);
	}
	
	pkcs7_mime = (GMimeApplicationPkcs7Mime *) message->mime_part;
	
	if (!(signatures = g_mime_application_pkcs7_mime_verify (pkcs7_mime, 0, &entity, &err))) {
		ex = exception_new ("verify failed: %s", err->message);
		g_object_unref (message);
		g_error_free (err);
		throw (ex);
	}
	
	v(print_verify_results (signatures));
	g_object_unref (signatures);
	g_object_unref (message);
	
	/* TODO: verify extracted content... */
	if (!GMIME_IS_TEXT_PART (entity)) {
		g_object_unref (entity);
		throw (exception_new ("extracted entity was not a text/plain part?"));
	}
	
	text = g_mime_text_part_get_text ((GMimeTextPart *) entity);
	if (strcmp (SIGNED_CONTENT, text) != 0) {
		ex = exception_new ("text part content does not match");
		g_object_unref (entity);
		g_free (text);
		throw (ex);
	}
	
	g_object_unref (entity);
	g_free (text);
}

#define ENCRYPTED_CONTENT "This is a test of application/pkcs7-mime; smime-type=enveloped-data.\n"

static void
test_pkcs7_mime_encrypt (void)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeDecryptResult *result;
	GPtrArray *recipients;
	GMimeMessage *message;
	GMimeObject *entity;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	char *text;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, ENCRYPTED_CONTENT);
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "mimekit@example.com");
	
	pkcs7_mime = g_mime_application_pkcs7_mime_encrypt ((GMimeObject *) part, GMIME_ENCRYPT_ALWAYS_TRUST, recipients, &err);
	g_ptr_array_free (recipients, TRUE);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("encrypt failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeObject *) pkcs7_mime);
	g_object_unref (pkcs7_mime);
	
	if (!GMIME_IS_APPLICATION_PKCS7_MIME (message->mime_part)) {
		ex = exception_new ("resultant top-level mime part not an application/pkcs7-mime?");
		g_object_unref (message);
		throw (ex);
	}
	
	pkcs7_mime = (GMimeApplicationPkcs7Mime *) message->mime_part;
	
	if (!(entity = g_mime_application_pkcs7_mime_decrypt (pkcs7_mime, 0, NULL, &result, &err))) {
		ex = exception_new ("decrypt failed: %s", err->message);
		g_object_unref (message);
		g_error_free (err);
		throw (ex);
	}
	
	g_object_unref (message);
	
	if (result->signatures) {
		g_object_unref (result);
		throw (exception_new ("signature status expected to be NONE"));
	}
	
	g_object_unref (result);
	
	/* TODO: verify decrypted content... */
	if (!GMIME_IS_TEXT_PART (entity)) {
		g_object_unref (entity);
		throw (exception_new ("decrypted entity was not a text/plain part?"));
	}
	
	text = g_mime_text_part_get_text ((GMimeTextPart *) entity);
	
	if (strcmp (ENCRYPTED_CONTENT, text) != 0) {
		ex = exception_new ("text part content does not match");
		g_object_unref (entity);
		g_free (text);
		throw (ex);
	}
	
	g_object_unref (entity);
	g_free (text);
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
	const char *datadir = "data/smime";
	GMimeCryptoContext *ctx;
	struct stat st;
	char *key;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (testsuite_setup_gpghome ("gpgsm") != 0)
		return EXIT_FAILURE;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return EXIT_FAILURE;
	
	testsuite_start ("S/MIME implementation");
	
	ctx = g_mime_pkcs7_context_new ();
	//g_mime_crypto_context_set_request_password (ctx, request_passwd);
	
	testsuite_check ("GMimePkcs7Context::import");
	try {
		key = g_build_filename (datadir, "certificate-authority.crt", NULL);
		import_key (ctx, key);
		g_free (key);
		
		key = g_build_filename (datadir, "smime.p12", NULL);
		import_key (ctx, key);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimePkcs7Context::import failed: %s", ex->message);
		return EXIT_FAILURE;
	} finally;
	
	testsuite_check ("multipart/signed");
	try {
		test_multipart_signed (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/signed failed: %s", ex->message);
	} finally;
	
	testsuite_check ("application/pkcs7-mime; smime-type=signed-data");
	try {
		test_pkcs7_mime_sign ();
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/signed failed: %s", ex->message);
	} finally;
	
	testsuite_check ("application/pkcs7-mime; smime-type=enveloped-data");
	try {
		test_pkcs7_mime_encrypt ();
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted failed: %s", ex->message);
	} finally;
	
	g_object_unref (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
}
