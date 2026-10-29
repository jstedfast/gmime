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

#ifdef ENABLE_CRYPTO
#include <gpgme.h>
#endif

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
	GMimeStreamMemPrivate *priv;
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_crypto_context_sign (ctx, detached, "no.user@no.domain", cleartext, ciphertext, &err);
	
	if (rv == -1 || err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ciphertext));

	v(fprintf (stderr, "signature (%s):\n%.*s\n",
		   g_mime_crypto_context_digest_name (ctx, rv),
		   priv->buffer->len,
		   priv->buffer->data));
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
	GMimeStreamMemPrivate *cleartext_priv;
	GMimeStreamMemPrivate *stream_priv;
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
test_encrypt (GMimeCryptoContext *ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeStreamMemPrivate *priv;
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
	
	priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ciphertext));

	v(fprintf (stderr, "ciphertext:\n%.*s\n",
		   priv->buffer->len,
		   priv->buffer->data));
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

#ifdef ENABLE_CRYPTO
	if (ex == NULL) {
		GMimeCertificateList *recipients = g_mime_decrypt_result_get_recipients (result);
		GMimeCertificate *cert;
		gpgme_key_t key;
		int i;

		for (i = 0; i < g_mime_certificate_list_length (recipients); i++) {
			cert = g_mime_certificate_list_get_certificate (recipients, i);
			key = g_mime_certificate_get_gpgme_key (cert);
			if (!key || !key->subkeys) {
				ex = exception_new ("decryption recipient has no backend key");
				break;
			}
		}
	}
#endif
	
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

#ifdef ENABLE_CRYPTO
static void
test_gpgme_handles (GMimeCryptoContext *ctx)
{
	static const char fingerprint[] = "DA26DAB7572A1C5F2AB1A74BB47760E95182C200";
	GMimeCryptoContext *owner = g_mime_gpg_context_new (), *pkcs7 = NULL;
	GMimeStream *input = g_mime_stream_mem_new_with_buffer ("backend handles", 15);
	GMimeStream *signed_data = g_mime_stream_mem_new ();
	GMimeSignatureList *signatures = NULL;
	GMimeCertificate *cert = NULL;
	gpgme_key_t key = NULL, retained = NULL, expected = NULL;
	gpgme_ctx_t backend;
	gpgme_user_id_t uid, expected_uid;
	GError *error = NULL;
	const char *failure = NULL;
	gboolean original = FALSE, alternate = FALSE, no_email = FALSE;
	guint count = 0;

	if (!owner || !(backend = g_mime_gpg_context_get_gpgme_ctx (GMIME_GPG_CONTEXT (owner))) ||
	    gpgme_get_protocol (backend) != GPGME_PROTOCOL_OpenPGP) {
		failure = "OpenPGP context does not expose its backend";
		goto done;
	}

	if (gpgme_engine_check_version (GPGME_PROTOCOL_CMS) == GPG_ERR_NO_ERROR) {
		pkcs7 = g_mime_pkcs7_context_new ();
		if (!pkcs7 || !g_mime_pkcs7_context_get_gpgme_ctx (GMIME_PKCS7_CONTEXT (pkcs7)) ||
		    gpgme_get_protocol (g_mime_pkcs7_context_get_gpgme_ctx (GMIME_PKCS7_CONTEXT (pkcs7))) !=
		    GPGME_PROTOCOL_CMS) {
			failure = "PKCS7 context does not expose its backend";
			goto done;
		}
	} else {
		testsuite_check_warn ("CMS backend handle check requires gpgsm");
	}

	if (g_mime_crypto_context_sign (ctx, TRUE, "no.user@no.domain", input, signed_data, &error) == -1) {
		failure = "could not sign backend-handle test data";
		goto done;
	}
	g_mime_stream_reset (input);
	g_mime_stream_reset (signed_data);
	signatures = g_mime_crypto_context_verify (owner, GMIME_VERIFY_NONE, input, signed_data, NULL, &error);
	if (!signatures || g_mime_signature_list_length (signatures) != 1 ||
	    (get_sig_status (signatures) & GMIME_SIGNATURE_STATUS_RED)) {
		failure = "multiple identities must still produce one valid signature";
		goto done;
	}

	cert = g_object_ref (g_mime_signature_get_certificate (g_mime_signature_list_get_signature (signatures, 0)));
	key = g_mime_certificate_get_gpgme_key (cert);
	if (!key || !key->subkeys || g_strcmp0 (key->subkeys->fpr, fingerprint) ||
	    backend != g_mime_gpg_context_get_gpgme_ctx (GMIME_GPG_CONTEXT (owner))) {
		failure = "signature certificate does not retain its backend key";
		goto done;
	}

	if (gpgme_get_key (backend, fingerprint, &expected, 0) != GPG_ERR_NO_ERROR || !expected) {
		failure = "could not look up the expected backend identity metadata";
		goto done;
	}
	expected_uid = expected->uids;
	for (uid = key->uids; uid; uid = uid->next) {
		count++;
		original |= g_strcmp0 (uid->email, "no.user@no.domain") == 0;
		alternate |= g_strcmp0 (uid->email, "other.user@no.domain") == 0 &&
			g_strcmp0 (uid->name, "Another User") == 0 &&
			g_strcmp0 (uid->comment, "second identity") == 0;
		no_email |= g_strcmp0 (uid->uid, "Test Identity Without Email") == 0 &&
			(!uid->email || !*uid->email);
		if (!expected_uid || g_strcmp0 (uid->uid, expected_uid->uid) ||
		    uid->validity != expected_uid->validity || uid->revoked != expected_uid->revoked ||
		    uid->invalid != expected_uid->invalid) {
			failure = "backend key did not preserve each identity's validity and status";
			goto done;
		}
		expected_uid = expected_uid->next;
	}
	if (count != 3 || !original || !alternate || !no_email || expected_uid) {
		failure = "backend key must expose all identities, including one without an email address";
		goto done;
	}
	gpgme_key_unref (expected);
	expected = NULL;

	g_mime_certificate_set_user_id (cert, "application override");
	if (g_strcmp0 (key->uids->uid, "application override") == 0 ||
	    g_mime_certificate_get_gpgme_key (cert) != key) {
		failure = "certificate setters must not modify the backend key";
		goto done;
	}

	if (gpgme_op_keylist_start (backend, NULL, 0) != GPG_ERR_NO_ERROR ||
	    gpgme_op_keylist_end (backend) != GPG_ERR_NO_ERROR) {
		failure = "borrowed backend context cannot be used for native operations";
		goto done;
	}
	g_clear_object (&signatures);
	g_clear_object (&owner);
	if (g_mime_certificate_get_gpgme_key (cert) != key || !key->uids ||
	    g_strcmp0 (key->subkeys->fpr, fingerprint)) {
		failure = "certificate key must survive later backend operations and context destruction";
		goto done;
	}

	retained = key;
	gpgme_key_ref (retained);
	g_clear_object (&cert);
	if (!retained->uids || g_strcmp0 (retained->subkeys->fpr, fingerprint))
		failure = "caller-referenced key must survive certificate destruction";

done:
	if (expected)
		gpgme_key_unref (expected);
	if (retained)
		gpgme_key_unref (retained);
	g_clear_object (&cert);
	g_clear_object (&signatures);
	g_clear_object (&owner);
	g_clear_object (&pkcs7);
	g_object_unref (signed_data);
	g_object_unref (input);
	if (failure) {
		Exception *ex = exception_new ("%s%s%s", failure, error ? ": " : "", error ? error->message : "");
		g_clear_error (&error);
		throw (ex);
	}
	g_clear_error (&error);
}
#endif

static void
test_export (GMimeCryptoContext *ctx, const char *path)
{
	GMimeStreamMemPrivate *istream_priv;
	GMimeStreamMemPrivate *ostream_priv;
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
	
	istream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (istream));

	inbuf = (const char *) istream_priv->buffer->data;
	inlen = istream_priv->buffer->len;
	if ((inptr = strstr (inbuf, "\n\n"))) {
		/* skip past the headers which may have different version numbers */
		inptr += 2;
		inlen -= (inptr - inbuf);
		inbuf = inptr;
	}
	
	ostream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ostream));

	outbuf = (const char *) ostream_priv->buffer->data;
	outlen = ostream_priv->buffer->len;
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
	GMimeStreamMemPrivate *expected_priv;
	GMimeStreamMemPrivate *ostream_priv;
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
	
	expected_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (expected));

	buf[0] = expected_priv->buffer;
	ostream_priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ostream));

	buf[1] = ostream_priv->buffer;
	
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
	
	if (testsuite_setup_gpghome (gpg, "pgp") != 0)
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

	testsuite_check ("GPGME backend handles and multi-identity key lifetime");
	try {
		key = g_build_filename (datadir, "gmime-multi-uid.gpg.pub", NULL);
		import_key (ctx, key);
		g_free (key);
		test_gpgme_handles (ctx);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME backend handles: %s", ex->message);
	} finally;

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
