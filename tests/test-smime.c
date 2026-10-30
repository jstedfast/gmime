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
#include "test-crypto.h"

extern int verbose;

#define v(x) if (verbose > 3) x

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
		g_error_free (err);
		throw (ex);
	}
	
	if (get_sig_status (signatures) & GPGME_SIGSUM_RED) {
		gpgme_result_unref (signatures);
		g_object_unref (message);
		throw (exception_new ("signature status was BAD"));
	}
	v(print_verify_results (signatures));
	gpgme_result_unref (signatures);
	
	g_object_unref (message);
}

#define SIGNED_CONTENT "This is a test of application/pkcs7-mime; smime-type=signed-data.\n"

static void
test_pkcs7_mime_sign (gpgme_ctx_t ctx)
{
	GMimeMessagePrivate *priv;
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	gpgme_verify_result_t signatures = NULL;
	GMimeMessage *message;
	GMimeEntity *entity = NULL;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	char *text;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, SIGNED_CONTENT);
	
	pkcs7_mime = g_mime_application_pkcs7_mime_sign (ctx, (GMimeEntity *) part, NULL, &err);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("sign failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeEntity *) pkcs7_mime);
	g_object_unref (pkcs7_mime);
	
	priv = _g_mime_message_get_private (message);

	if (!GMIME_IS_APPLICATION_PKCS7_MIME (priv->mime_part)) {
		ex = exception_new ("resultant top-level mime part not an application/pkcs7-mime?");
		g_object_unref (message);
		throw (ex);
	}
	
	pkcs7_mime = (GMimeApplicationPkcs7Mime *) priv->mime_part;
	
	if (!g_mime_application_pkcs7_mime_verify (pkcs7_mime, ctx, &entity, &signatures, &err)) {
		g_clear_pointer (&signatures, gpgme_result_unref);
		g_clear_object (&entity);
		ex = exception_new ("verify failed: %s", err->message);
		g_object_unref (message);
		g_error_free (err);
		throw (ex);
	}
	
	if (get_sig_status (signatures) & GPGME_SIGSUM_RED) {
		gpgme_result_unref (signatures);
		g_object_unref (message);
		g_object_unref (entity);
		throw (exception_new ("signature status was BAD"));
	}
	v(print_verify_results (signatures));
	gpgme_result_unref (signatures);
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
test_pkcs7_mime_encrypt (gpgme_ctx_t ctx)
{
	GMimeMessagePrivate *priv;
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	gpgme_decrypt_result_t result = NULL;
	gpgme_key_t recipients[2] = { NULL, NULL };
	GMimeMessage *message;
	GMimeEntity *entity;
	GMimeTextPart *part;
	GError *err = NULL;
	Exception *ex;
	char *text;
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, ENCRYPTED_CONTENT);
	
	recipients[0] = test_crypto_key (ctx, "mimekit@example.com", FALSE);
	
	pkcs7_mime = g_mime_application_pkcs7_mime_encrypt (ctx, (GMimeEntity *) part, GPGME_ENCRYPT_ALWAYS_TRUST, recipients, NULL, &err);
	gpgme_key_unref (recipients[0]);
	g_object_unref (part);
	
	if (err != NULL) {
		ex = exception_new ("encrypt failed: %s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	message = create_message ((GMimeEntity *) pkcs7_mime);
	g_object_unref (pkcs7_mime);
	
	priv = _g_mime_message_get_private (message);

	if (!GMIME_IS_APPLICATION_PKCS7_MIME (priv->mime_part)) {
		ex = exception_new ("resultant top-level mime part not an application/pkcs7-mime?");
		g_object_unref (message);
		throw (ex);
	}
	
	pkcs7_mime = (GMimeApplicationPkcs7Mime *) priv->mime_part;
	
	if (!(entity = g_mime_application_pkcs7_mime_decrypt (pkcs7_mime, ctx, &result, &err))) {
		g_clear_pointer (&result, gpgme_result_unref);
		ex = exception_new ("decrypt failed: %s", err->message);
		g_object_unref (message);
		g_error_free (err);
		throw (ex);
	}
	
	g_object_unref (message);
	
	
	gpgme_result_unref (result);
	
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

int main (int argc, char *argv[])
{
	const char *datadir = "data/smime";
	gpgme_ctx_t ctx;
	struct stat st;
	char *volatile key;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (testsuite_setup_gpghome ("gpgsm", "smime") != 0)
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
	
	ctx = test_crypto_context (GPGME_PROTOCOL_CMS);

	
	testsuite_check ("GPGME CMS::import");
	try {
		key = g_build_filename (datadir, "certificate-authority.crt", NULL);
		import_key (ctx, key);
		g_free (key);
		
		key = g_build_filename (datadir, "smime.p12", NULL);
		import_key (ctx, key);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME CMS::import failed: %s", ex->message);
		exception_free (ex);
		g_free (key);
		gpgme_release (ctx);
		testsuite_end ();
		g_mime_shutdown ();
		testsuite_destroy_gpghome ();
		return EXIT_FAILURE;
	} finally;
	
	test_crypto_signer (ctx, "mimekit@example.com");

	testsuite_check ("multipart/signed");
	try {
		test_multipart_signed (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/signed failed: %s", ex->message);
	} finally;
	
	testsuite_check ("application/pkcs7-mime; smime-type=signed-data");
	try {
		test_pkcs7_mime_sign (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/signed failed: %s", ex->message);
	} finally;
	
	testsuite_check ("application/pkcs7-mime; smime-type=enveloped-data");
	try {
		test_pkcs7_mime_encrypt (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("multipart/encrypted failed: %s", ex->message);
	} finally;
	
	gpgme_release (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
}
