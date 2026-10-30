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


#include <gpg-error.h>

#include "testsuite.h"
#include "test-crypto.h"

extern int verbose;

#define v(x) if (verbose > 3) x

static void
test_sign (gpgme_ctx_t ctx, gboolean detached, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_gpgme_sign (ctx, detached ? GPGME_SIG_MODE_DETACH : GPGME_SIG_MODE_NORMAL, cleartext, ciphertext, NULL, &err);
	
	if (!rv || err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}

static void
test_verify_detached (gpgme_ctx_t ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	gpgme_verify_result_t signatures = NULL;
	gpgme_sigsum_t status;
	GError *err = NULL;
	Exception *ex;
	
	if (!g_mime_gpgme_verify (ctx, cleartext, ciphertext, NULL, &signatures, &err)) {
		g_clear_pointer (&signatures, gpgme_result_unref);
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	status = get_sig_status (signatures);
	
	if ((status & GPGME_SIGSUM_RED) != 0) {
		gpgme_result_unref (signatures);
		throw (exception_new ("signature BAD"));
	}
	
	gpgme_result_unref (signatures);
}

static void
test_verify (gpgme_ctx_t ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeStreamMemPrivate *cleartext_priv;
	GMimeStreamMemPrivate *stream_priv;
	gpgme_verify_result_t signatures = NULL;
	gpgme_sigsum_t status;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	if (!g_mime_gpgme_verify (ctx, ciphertext, NULL, stream, &signatures, &err)) {
		g_clear_pointer (&signatures, gpgme_result_unref);
		ex = exception_new ("%s", err->message);
		g_object_unref (stream);
		g_error_free (err);
		throw (ex);
	}
	
	status = get_sig_status (signatures);
	
	if ((status & GPGME_SIGSUM_RED) != 0) {
		gpgme_result_unref (signatures);
		g_object_unref (stream);
		
		throw (exception_new ("signature BAD"));
	}
	
	gpgme_result_unref (signatures);
	
	cleartext_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (cleartext));

	buf[0] = cleartext_priv->buffer;
	stream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (stream));

	buf[1] = stream_priv->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("extracted data does not match original cleartext");
	
	g_object_unref (stream);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_encrypt (gpgme_ctx_t ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	gpgme_key_t recipients[2] = { NULL, NULL };
	GError *err = NULL;
	Exception *ex;
	
	recipients[0] = test_crypto_key (ctx, "mimekit@example.com", FALSE);
	
	g_mime_gpgme_encrypt (ctx, FALSE, 0, recipients, cleartext, ciphertext, NULL, NULL, &err);
	
	gpgme_key_unref (recipients[0]);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}

static void
test_decrypt (gpgme_ctx_t ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeStreamMemPrivate *cleartext_priv;
	GMimeStreamMemPrivate *stream_priv;
	gpgme_recipient_t recipients;
	gpgme_decrypt_result_t result = NULL;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	if (!g_mime_gpgme_decrypt (ctx, FALSE, ciphertext, stream, &result, NULL, &err)) {
		g_clear_pointer (&result, gpgme_result_unref);
		g_object_unref (stream);
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	if (!(recipients = result->recipients)) {
		gpgme_result_unref (result);
		g_object_unref (stream);
		
		throw (exception_new ("Failed to get recipients"));
	}
	
	gpgme_result_unref (result);
	
	if (ex != NULL) {
		g_object_unref (stream);
		throw (ex);
	}
	
	cleartext_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (cleartext));

	buf[0] = cleartext_priv->buffer;
	stream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (stream));

	buf[1] = stream_priv->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("decrypted data does not match original cleartext");
	
	g_object_unref (stream);
	
	if (ex != NULL)
		throw (ex);
}

static void
test_export (gpgme_ctx_t ctx, const char *path)
{
	GMimeStreamMemPrivate *istream_priv;
	GMimeStreamMemPrivate *ostream_priv;
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
	
	test_crypto_export (ctx, keys, ostream, &err);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_object_unref (istream);
		g_object_unref (ostream);
		g_error_free (err);
		throw (ex);
	}
	
	istream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (istream));

	inbuf = (const char *) istream_priv->buffer->data;
	inlen = istream_priv->buffer->len;
	
	ostream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ostream));

	outbuf = (const char *) ostream_priv->buffer->data;
	outlen = ostream_priv->buffer->len;
	
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


int main (int argc, char **argv)
{
	const char *datadir = "data/smime";
	GMimeStream *istream, *ostream;
	gpgme_ctx_t ctx;
	const char *what;
	struct stat st;
	char *volatile key;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if (testsuite_setup_gpghome ("gpgsm", "pkcs7") != 0)
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
	
	ctx = test_crypto_context (GPGME_PROTOCOL_CMS);

	
	testsuite_check ("GPGME CMS::import");
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

	key = g_build_filename (datadir, "smime.crt", NULL);
	testsuite_check ("GPGME CMS::export");
	try {
		test_export (ctx, key);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME CMS::export failed: %s", ex->message);
	} finally;
	
	g_free (key);
	
	istream = g_mime_stream_mem_new ();
	ostream = g_mime_stream_mem_new ();
	
	g_mime_stream_write_string (istream, "this is some cleartext\r\n");
	g_mime_stream_reset (istream);
	
	what = "GPGME CMS::sign";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME CMS::verify";
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
	
	what = "GPGME CMS::sign (detached)";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME CMS::verify (detached)";
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
	
	what = "GPGME CMS::encrypt";
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME CMS::decrypt";
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
	gpgme_release (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (testsuite_destroy_gpghome () != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
}
