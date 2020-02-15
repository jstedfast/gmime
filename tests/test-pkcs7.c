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

#include <gpg-error.h>

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
test_sign (GMimeCryptoContext *ctx, gboolean detached, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_crypto_context_sign (ctx, detached, "mimekit@example.com", cleartext, ciphertext, &err);
	
	if (rv == -1 || err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}

static void
test_verify_detached (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeSignatureList *signatures;
	GMimeSignatureStatus status;
	GError *err = NULL;
	Exception *ex;
	
	signatures = g_mime_crypto_context_verify (ctx, 0, cleartext, ciphertext, NULL, &err);
	
	if (signatures == NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	status = get_sig_status (signatures);
	
	if ((status & GMIME_SIGNATURE_STATUS_RED) != 0) {
		g_object_unref (signatures);
		throw (exception_new ("signature BAD"));
	}
	
	g_object_unref (signatures);
}

static void
test_verify (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeSignatureList *signatures;
	GMimeSignatureStatus status;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	signatures = g_mime_crypto_context_verify (ctx, 0, ciphertext, NULL, stream, &err);
	
	if (signatures == NULL) {
		ex = exception_new ("%s", err->message);
		g_object_unref (stream);
		g_error_free (err);
		throw (ex);
	}
	
	status = get_sig_status (signatures);
	
	if ((status & GMIME_SIGNATURE_STATUS_RED) != 0) {
		g_object_unref (signatures);
		g_object_unref (stream);
		
		throw (exception_new ("signature BAD"));
	}
	
	g_object_unref (signatures);
	
	buf[0] = GMIME_STREAM_MEM (cleartext)->buffer;
	buf[1] = GMIME_STREAM_MEM (stream)->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("extracted data does not match original cleartext");
	
	g_object_unref (stream);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_encrypt (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GPtrArray *recipients;
	GError *err = NULL;
	Exception *ex;
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "mimekit@example.com");
	
	g_mime_crypto_context_encrypt (ctx, FALSE, NULL, GMIME_ENCRYPT_NONE,
				       recipients, cleartext, ciphertext, &err);
	
	g_ptr_array_free (recipients, TRUE);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}

static void
test_decrypt (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeCertificateList *recipients;
	GMimeDecryptResult *result;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	if (!(result = g_mime_crypto_context_decrypt (ctx, 0, NULL, ciphertext, stream, &err))) {
		g_object_unref (stream);
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	if (!(recipients = g_mime_decrypt_result_get_recipients (result))) {
		g_object_unref (result);
		g_object_unref (stream);
		
		throw (exception_new ("Failed to get recipients"));
	}
	
	g_object_unref (result);
	
	if (ex != NULL) {
		g_object_unref (stream);
		throw (ex);
	}
	
	buf[0] = GMIME_STREAM_MEM (cleartext)->buffer;
	buf[1] = GMIME_STREAM_MEM (stream)->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	
	g_object_unref (stream);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_export (GMimeCryptoContext *ctx, const char *path)
{
	GMimeStream *istream, *ostream;
	const char *inbuf, *outbuf;
	size_t inlen, outlen;
	Exception *ex = NULL;
	const char *keys[2];
	GError *err = NULL;
	int fd;
	
	if ((fd = open (path, O_RDONLY, 0)) == -1)
		throw (exception_new ("open() failed: %s", g_strerror (errno)));
	
	ostream = g_mime_stream_fs_new (fd);
	istream = g_mime_stream_mem_new ();
	g_mime_stream_write_to_stream (ostream, istream);
	g_mime_stream_reset (istream);
	g_object_unref (ostream);
	
	keys[0] = "mimekit@example.com";
	keys[1] = NULL;
	
	ostream = g_mime_stream_mem_new ();
	
	g_mime_crypto_context_export_keys (ctx, keys, ostream, &err);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_object_unref (istream);
		g_object_unref (ostream);
		g_error_free (err);
		throw (ex);
	}
	
	inbuf = (const char *) GMIME_STREAM_MEM (istream)->buffer->data;
	inlen = GMIME_STREAM_MEM (istream)->buffer->len;
	
	outbuf = (const char *) GMIME_STREAM_MEM (ostream)->buffer->data;
	outlen = GMIME_STREAM_MEM (ostream)->buffer->len;
	
	if (outlen != inlen || memcmp (outbuf, inbuf, inlen) != 0) {
		FILE *fp = fopen ("exported.crt", "w");
		fwrite (outbuf, 1, outlen, fp);
		fflush (fp);
		fclose (fp);
		
		ex = exception_new ("exported key does not match original key");
	}
	
	g_object_unref (istream);
	g_object_unref (ostream);
	
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
		ex = exception_new ("%s: %s", err->message, gpg_strerror (err->code));
		g_error_free (err);
		throw (ex);
	}
}


int main (int argc, char **argv)
{
	const char *datadir = "data/smime";
	GMimeStream *istream, *ostream;
	GMimeCryptoContext *ctx;
	const char *what;
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
	
	testsuite_start ("Pkcs7 crypto context");
	
	ctx = g_mime_pkcs7_context_new ();
	//g_mime_crypto_context_set_request_password (ctx, request_passwd);
	
	testsuite_check ("GMimePkcs7Context::import");
	try {
		key = g_build_filename (datadir, "certificate-authority.crt", NULL);
		//printf ("importing key: %s\n", key);
		import_key (ctx, key);
		g_free (key);
		
		key = g_build_filename (datadir, "smime.p12", NULL);
		//printf ("importing key: %s\n", key);
		import_key (ctx, key);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimePkcs7Context::import failed: %s", ex->message);
		return EXIT_FAILURE;
	} finally;
	
	key = g_build_filename (datadir, "smime.crt", NULL);
	testsuite_check ("GMimePkcs7Context::export");
	try {
		test_export (ctx, key);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimePkcs7Context::export failed: %s", ex->message);
	} finally;
	
	g_free (key);
	
	istream = g_mime_stream_mem_new ();
	ostream = g_mime_stream_mem_new ();
	
	g_mime_stream_write_string (istream, "this is some cleartext\r\n");
	g_mime_stream_reset (istream);
	
	what = "GMimePkcs7Context::sign";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimePkcs7Context::verify";
		testsuite_check ("%s", what);
		g_mime_stream_reset (istream);
		g_mime_stream_reset (ostream);
		test_verify (ctx, istream, ostream);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (ostream);
	g_mime_stream_reset (istream);
	ostream = g_mime_stream_mem_new ();
	
	what = "GMimePkcs7Context::sign (detached)";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimePkcs7Context::verify (detached)";
		testsuite_check ("%s", what);
		g_mime_stream_reset (istream);
		g_mime_stream_reset (ostream);
		test_verify_detached (ctx, istream, ostream);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (ostream);
	g_mime_stream_reset (istream);
	ostream = g_mime_stream_mem_new ();
	
	what = "GMimePkcs7Context::encrypt";
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimePkcs7Context::decrypt";
		testsuite_check ("%s", what);
		g_mime_stream_reset (istream);
		g_mime_stream_reset (ostream);
		test_decrypt (ctx, istream, ostream);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (istream);
	g_object_unref (ostream);
	g_object_unref (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
}
