/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

static GMimeSignatureStatus
get_sig_status (GMimeSignatureList *signatures)
{
	GMimeSignatureStatus status = GMIME_SIGNATURE_STATUS_GOOD;
	GMimeSignature *sig;
	int i;
	
	if (!signatures || signatures->array->len == 0)
		return GMIME_SIGNATURE_STATUS_ERROR;
	
	for (i = 0; i < g_mime_signature_list_length (signatures); i++) {
		sig = g_mime_signature_list_get_signature (signatures, i);
		status = MAX (status, sig->status);
	}
	
	return status;
}

static void
test_sign (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_crypto_context_sign (ctx, "no.user@no.domain",
					 GMIME_DIGEST_ALGO_SHA256,
					 cleartext, ciphertext, &err);
	
	if (rv == -1 || err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	v(fprintf (stderr, "signature (%s):\n%.*s\n",
		   g_mime_crypto_context_digest_name (ctx, rv),
		   GMIME_STREAM_MEM (ciphertext)->buffer->len,
		   GMIME_STREAM_MEM (ciphertext)->buffer->data));
}

static void
test_verify (GMimeCryptoContext *ctx, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeSignatureList *signatures;
	GError *err = NULL;
	Exception *ex;
	
	signatures = g_mime_crypto_context_verify (ctx, GMIME_DIGEST_ALGO_DEFAULT,
						   cleartext, ciphertext, &err);
	
	if (signatures == NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	if (get_sig_status (signatures) != GMIME_SIGNATURE_STATUS_GOOD) {
		g_object_unref (signatures);
		throw (exception_new ("signature BAD"));
	}
	
	g_object_unref (signatures);
}

static void
test_encrypt (GMimeCryptoContext *ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GPtrArray *recipients;
	GError *err = NULL;
	Exception *ex;
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	
	g_mime_crypto_context_encrypt (ctx, sign, "no.user@no.domain", GMIME_DIGEST_ALGO_SHA256,
				       recipients, cleartext, ciphertext, &err);
	
	g_ptr_array_free (recipients, TRUE);
	
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	v(fprintf (stderr, "ciphertext:\n%.*s\n",
		   GMIME_STREAM_MEM (ciphertext)->buffer->len,
		   GMIME_STREAM_MEM (ciphertext)->buffer->data));
}

static void
test_decrypt (GMimeCryptoContext *ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeDecryptResult *result;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	if (!(result = g_mime_crypto_context_decrypt (ctx, ciphertext, stream, &err))) {
		g_object_unref (stream);
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	if (sign) {
		if (!result->signatures || get_sig_status (result->signatures) != GMIME_SIGNATURE_STATUS_GOOD)
			ex = exception_new ("expected GOOD signature");
	} else {
		if (result->signatures != NULL)
			ex = exception_new ("unexpected signature");
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
	register const char *inptr;
	const char *inbuf, *outbuf;
	size_t inlen, outlen;
	Exception *ex = NULL;
	GError *err = NULL;
	GPtrArray *keys;
	int fd;
	
	if ((fd = open (path, O_RDONLY, 0)) == -1)
		throw (exception_new ("open() failed: %s", g_strerror (errno)));
	
	ostream = g_mime_stream_fs_new (fd);
	istream = g_mime_stream_mem_new ();
	g_mime_stream_write_to_stream (ostream, istream);
	g_mime_stream_reset (istream);
	g_object_unref (ostream);
	
	keys = g_ptr_array_new ();
	g_ptr_array_add (keys, "no.user@no.domain");
	
	ostream = g_mime_stream_mem_new ();
	
	g_mime_crypto_context_export_keys (ctx, keys, ostream, &err);
	g_ptr_array_free (keys, TRUE);
	if (err != NULL) {
		ex = exception_new ("%s", err->message);
		g_object_unref (istream);
		g_object_unref (ostream);
		g_error_free (err);
		throw (ex);
	}
	
	inbuf = (const char *) GMIME_STREAM_MEM (istream)->buffer->data;
	inlen = GMIME_STREAM_MEM (istream)->buffer->len;
	if ((inptr = strstr (inbuf, "\n\n"))) {
		/* skip past the headers which may have different version numbers */
		inptr += 2;
		inlen -= (inptr - inbuf);
		inbuf = inptr;
	}
	
	outbuf = (const char *) GMIME_STREAM_MEM (ostream)->buffer->data;
	outlen = GMIME_STREAM_MEM (ostream)->buffer->len;
	if ((inptr = strstr (outbuf, "\n\n"))) {
		/* skip past the headers which may have different version numbers */
		inptr += 2;
		outlen -= (inptr - outbuf);
		outbuf = inptr;
	}
	
	if (outlen != inlen || memcmp (outbuf, inbuf, inlen) != 0)
		ex = exception_new ("exported key does not match original key");
	
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
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
}


int main (int argc, char **argv)
{
	const char *datadir = "data/pgp";
	GMimeStream *istream, *ostream;
	GMimeCryptoContext *ctx;
	const char *what;
	struct stat st;
	char *key;
	int i;
	
	g_mime_init (0);
	
	testsuite_init (argc, argv);
	
	/* reset .gnupg config directory */
	if (system ("/bin/rm -rf ./tmp") != 0)
		return EXIT_FAILURE;
	if (system ("/bin/mkdir ./tmp") != 0)
		return EXIT_FAILURE;
	setenv ("GNUPGHOME", "./tmp/.gnupg", 1);
	if (system ("/usr/bin/gpg --list-keys > /dev/null 2>&1") != 0)
		return EXIT_FAILURE;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("GnuPG crypto context");
	
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
	
	key = g_build_filename (datadir, "gmime.gpg.pub", NULL);
	testsuite_check ("GMimeGpgContext::export");
	try {
		test_export (ctx, key);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeGpgContext::export failed: %s", ex->message);
	} finally;
	
	g_free (key);
	
	istream = g_mime_stream_mem_new ();
	ostream = g_mime_stream_mem_new ();
	
	g_mime_stream_write_string (istream, "this is some cleartext\r\n");
	g_mime_stream_reset (istream);
	
	what = "GMimeGpgContext::sign";
	testsuite_check (what);
	try {
		test_sign (ctx, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::verify";
		testsuite_check (what);
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
	
	what = "GMimeGpgContext::encrypt";
	testsuite_check (what);
	try {
		test_encrypt (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::decrypt";
		testsuite_check (what);
		g_mime_stream_reset (istream);
		g_mime_stream_reset (ostream);
		test_decrypt (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (ostream);
	g_mime_stream_reset (istream);
	ostream = g_mime_stream_mem_new ();
	
	what = "GMimeGpgContext::encrypt+sign";
	testsuite_check (what);
	try {
		test_encrypt (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::decrypt+verify";
		testsuite_check (what);
		g_mime_stream_reset (istream);
		g_mime_stream_reset (ostream);
		test_decrypt (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (istream);
	g_object_unref (ostream);
	g_object_unref (ctx);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	if (system ("/bin/rm -rf ./tmp") != 0)
		return EXIT_FAILURE;
	
	return testsuite_exit ();
}
