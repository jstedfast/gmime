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
test_sign (GMimeCryptoContext *ctx, gboolean detached, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_crypto_context_sign (ctx, detached, "no.user@no.domain", cleartext, ciphertext, &err);
	
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
test_encrypt (GMimeCryptoContext *ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GPtrArray *recipients;
	GError *err = NULL;
	Exception *ex;
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	
	g_mime_crypto_context_encrypt (ctx, sign, "no.user@no.domain",
				       GMIME_ENCRYPT_ALWAYS_TRUST,
				       recipients, cleartext, ciphertext,
				       &err);
	
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
	GMimeSignatureList *signatures;
	GMimeSignatureStatus status;
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
	
	signatures = g_mime_decrypt_result_get_signatures (result);
	
	if (sign) {
		if (signatures != NULL) {
			status = get_sig_status (signatures);
			
			if ((status & GMIME_SIGNATURE_STATUS_RED) != 0)
				ex = exception_new ("expected GOOD signature");
		} else {
			ex = exception_new ("Failed to get signatures");
		}
	} else {
		if (signatures != NULL)
			ex = exception_new ("unexpected signature");
	}
	
	/* Did not ask for session_key -- it should not be present.
	   We test asking for session_key over in test-pgpmime.c */
	if (ex == NULL && result->session_key)
		ex = exception_new ("got session_key when not requested");
	
	g_object_unref (result);
	
	if (ex != NULL) {
		g_object_unref (stream);
		throw (ex);
	}
	
	buf[0] = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) cleartext);
	buf[1] = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	
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
	
	keys[0] = "no.user@no.domain";
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

static void
pump_data_through_filter (GMimeFilter *filter, const char *path, GMimeStream *ostream)
{
	GMimeStream *onebyte, *filtered, *stream;
	GMimeFilter *unix2dos, *dos2unix;
	
	filtered = g_mime_stream_filter_new (ostream);
	
	/* convert to DOS format before piping through the OpenPGP filter to maximize testing area */
	unix2dos = g_mime_filter_unix2dos_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, unix2dos);
	g_object_unref (unix2dos);
	
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	
	/* convert back to UNIX format after filtering */
	dos2unix = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, dos2unix);
	g_object_unref (dos2unix);
	
	onebyte = test_stream_onebyte_new (filtered);
	g_object_unref (filtered);
	
	stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
	g_mime_stream_write_to_stream (stream, onebyte);
	g_mime_stream_flush (onebyte);
	g_object_unref (onebyte);
	g_object_unref (stream);
}

static char *openpgp_data_types[] = {
	"GMIME_OPENPGP_DATA_NONE",
	"GMIME_OPENPGP_DATA_ENCRYPTED",
	"GMIME_OPENPGP_DATA_SIGNED",
	"GMIME_OPENPGP_DATA_PUBLIC_KEY",
	"GMIME_OPENPGP_DATA_PRIVATE_KEY"
};

static void
test_openpgp_filter (GMimeFilterOpenPGP *filter, const char *path, GMimeOpenPGPData data_type, gint64 begin, gint64 end)
{
	GMimeStream *filtered, *stream, *expected, *ostream;
	GMimeFilter *dos2unix;
	GMimeOpenPGPData type;
	Exception *ex = NULL;
	GByteArray *buf[2];
	char *filename;
	struct stat st;
	gint64 offset;
	
	ostream = g_mime_stream_mem_new ();
	
	pump_data_through_filter ((GMimeFilter *) filter, path, ostream);
	
	if ((type = g_mime_filter_openpgp_get_data_type (filter)) != data_type) {
		g_object_unref (ostream);
		throw (exception_new ("Incorrect OpenPGP data type detected: %s", openpgp_data_types[type]));
	}
	
	if ((offset = g_mime_filter_openpgp_get_begin_offset (filter)) != begin) {
		g_object_unref (ostream);
		throw (exception_new ("Incorrect begin offset: %ld", (long) offset));
	}
	
	if ((offset = g_mime_filter_openpgp_get_end_offset (filter)) != end) {
		g_object_unref (ostream);
		throw (exception_new ("Incorrect end offset: %ld", (long) offset));
	}
	
	filename = g_strdup_printf ("%s.openpgp-block", path);
	if (stat (filename, &st) == -1) {
		stream = g_mime_stream_fs_open (filename, O_RDWR | O_CREAT, 0644, NULL);
		g_mime_stream_reset (ostream);
		g_mime_stream_write_to_stream (ostream, stream);
		g_mime_stream_flush (stream);
		g_mime_stream_reset (stream);
	} else {
		stream = g_mime_stream_fs_open (filename, O_RDONLY, 0644, NULL);
	}
	g_free (filename);
	
	/* make sure the data is in UNIX format before comparing (might be running tests on Windows) */
	expected = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (expected);
	dos2unix = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, dos2unix);
	g_object_unref (dos2unix);
	
	g_mime_stream_write_to_stream (stream, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (stream);
	
	buf[0] = GMIME_STREAM_MEM (expected)->buffer;
	buf[1] = GMIME_STREAM_MEM (ostream)->buffer;
	
	if (buf[0]->len != buf[1]->len || memcmp (buf[0]->data, buf[1]->data, buf[0]->len) != 0)
		ex = exception_new ("filtered data does not match the expected result");
	
	g_object_unref (expected);
	g_object_unref (ostream);
	
	if (ex != NULL)
		throw (ex);
}

int main (int argc, char **argv)
{
#ifdef ENABLE_CRYPTO
	const char *datadir = "data/pgp";
	GMimeStream *istream, *ostream;
	GMimeFilterOpenPGP *filter;
	GMimeCryptoContext *ctx;
	const char *what;
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
	
	g_free (gpg);
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("GnuPG crypto context");
	
	ctx = g_mime_gpg_context_new ();
	g_mime_crypto_context_set_request_password (ctx, request_passwd);
	
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
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::verify";
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
	
	what = "GMimeGpgContext::sign (detached)";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::verify (detached)";
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
	
	what = "GMimeGpgContext::encrypt";
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::decrypt";
		testsuite_check ("%s", what);
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
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GMimeGpgContext::decrypt+verify";
		testsuite_check ("%s", what);
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

	filter = (GMimeFilterOpenPGP *) g_mime_filter_openpgp_new ();
	
	what = "GMimeFilterOpenPGP::public key block";
	testsuite_check ("%s", what);
	try {
		key = g_build_filename (datadir, "gmime.gpg.pub", NULL);
		test_openpgp_filter (filter, key, GMIME_OPENPGP_DATA_PUBLIC_KEY, 0, 1720);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_mime_filter_reset ((GMimeFilter *) filter);
	
	what = "GMimeFilterOpenPGP::private key block";
	testsuite_check ("%s", what);
	try {
		key = g_build_filename (datadir, "gmime.gpg.sec", NULL);
		test_openpgp_filter (filter, key, GMIME_OPENPGP_DATA_PRIVATE_KEY, 0, 1928);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_mime_filter_reset ((GMimeFilter *) filter);
	
	what = "GMimeFilterOpenPGP::signed message block";
	testsuite_check ("%s", what);
	try {
		key = g_build_filename (datadir, "signed-message.txt", NULL);
		test_openpgp_filter (filter, key, GMIME_OPENPGP_DATA_SIGNED, 162, 440);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_mime_filter_reset ((GMimeFilter *) filter);
	
	what = "GMimeFilterOpenPGP::encrypted message block";
	testsuite_check ("%s", what);
	try {
		key = g_build_filename (datadir, "encrypted-message.txt", NULL);
		test_openpgp_filter (filter, key, GMIME_OPENPGP_DATA_ENCRYPTED, 165, 1084);
		g_free (key);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s failed: %s", what, ex->message);
	} finally;
	
	g_object_unref (filter);
	
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
