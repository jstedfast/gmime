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
static void
test_sign (gpgme_ctx_t ctx, gboolean detached, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeStreamMemPrivate *priv;
	GError *err = NULL;
	Exception *ex;
	int rv;
	
	rv = g_mime_gpgme_sign (ctx, detached ? GPGME_SIG_MODE_DETACH : GPGME_SIG_MODE_NORMAL, cleartext, ciphertext, NULL, &err);
	
	if (!rv || err != NULL) {
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	
	priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (ciphertext));

	v(fprintf (stderr, "signature (%s):\n%.*s\n",
		   detached ? "detached" : "normal",
		   priv->buffer->len,
		   priv->buffer->data));
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
test_encrypt (gpgme_ctx_t ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	GMimeStreamMemPrivate *priv;
	gpgme_key_t recipients[2] = { NULL, NULL };
	GError *err = NULL;
	Exception *ex;
	
	recipients[0] = test_crypto_key (ctx, "no.user@no.domain", FALSE);
	
	g_mime_gpgme_encrypt (ctx, sign, GPGME_ENCRYPT_ALWAYS_TRUST, recipients, cleartext, ciphertext, NULL, NULL, &err);
	
	gpgme_key_unref (recipients[0]);
	
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
test_decrypt (gpgme_ctx_t ctx, gboolean sign, GMimeStream *cleartext, GMimeStream *ciphertext)
{
	gpgme_verify_result_t signatures = NULL;
	gpgme_sigsum_t status;
	gpgme_decrypt_result_t result = NULL;
	Exception *ex = NULL;
	GMimeStream *stream;
	GError *err = NULL;
	GByteArray *buf[2];
	
	stream = g_mime_stream_mem_new ();
	
	if (!g_mime_gpgme_decrypt (ctx, TRUE, ciphertext, stream, &result, &signatures, &err)) {
		g_clear_pointer (&result, gpgme_result_unref);
		g_clear_pointer (&signatures, gpgme_result_unref);
		g_object_unref (stream);
		ex = exception_new ("%s", err->message);
		g_error_free (err);
		throw (ex);
	}
	if (sign) {
		if (signatures != NULL) {
			status = get_sig_status (signatures);
			
			if ((status & GPGME_SIGSUM_RED) != 0)
				ex = exception_new ("expected GOOD signature");
		} else {
			ex = exception_new ("Failed to get signatures");
		}
	} else {
		if (signatures && signatures->signatures)
			ex = exception_new ("unexpected signature");
	}
	
	/* Did not ask for session_key -- it should not be present.
	   We test asking for session_key over in test-pgpmime.c */
	if (ex == NULL && result->session_key)
		ex = exception_new ("got session_key when not requested");

	if (ex == NULL) {
		gpgme_recipient_t recipient;
		for (recipient = result->recipients; recipient; recipient = recipient->next)
			if (!recipient->keyid || recipient->status)
				ex = exception_new ("invalid native decryption recipient");
	}
	if (signatures)
		gpgme_result_unref (signatures);

	gpgme_result_unref (result);
	
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
test_gpgme_handles (void)
{
	static const char fingerprint[] = "DA26DAB7572A1C5F2AB1A74BB47760E95182C200";
	gpgme_ctx_t ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);
	gpgme_key_t key = test_crypto_key (ctx, fingerprint, TRUE);
	gpgme_key_t recipients[] = { key, NULL }, signer;
	gpgme_user_id_t uid;
	gpgme_sign_result_t signed_result = NULL, encrypted_sign = NULL;
	gpgme_verify_result_t verified = NULL, decrypted_verify = NULL, invalid = NULL;
	gpgme_encrypt_result_t encrypted_result = NULL;
	gpgme_decrypt_result_t decrypted_result = NULL;
	GMimeStream *input = g_mime_stream_mem_new_with_buffer ("backend handles", 15);
	GMimeStream *signature = g_mime_stream_mem_new ();
	GMimeStream *encrypted = g_mime_stream_mem_new ();
	GMimeStream *output = g_mime_stream_mem_new ();
	GMimeStream *bad = g_mime_stream_mem_new_with_buffer ("tampered", 8);
	GMimeStream *owned, *weak;
	gpgme_data_t data;
	GError *error = NULL;
	gboolean original = FALSE, alternate = FALSE, no_email = FALSE;
	guint count = 0, armor, textmode;
	char *signature_fingerprint;

	for (uid = key->uids; uid; uid = uid->next) {
		count++;
		original |= g_strcmp0 (uid->email, "no.user@no.domain") == 0;
		alternate |= g_strcmp0 (uid->email, "other.user@no.domain") == 0 &&
			g_strcmp0 (uid->name, "Another User") == 0 &&
			g_strcmp0 (uid->comment, "second identity") == 0;
		no_email |= g_strcmp0 (uid->uid, "Test Identity Without Email") == 0 &&
			(!uid->email || !*uid->email);
	}
	g_assert_cmpuint (count, ==, 3);
	g_assert_true (original && alternate && no_email);
	g_assert_cmpuint (gpgme_signers_add (ctx, key), ==, 0);

	owned = weak = g_mime_stream_mem_new_with_buffer ("adapter", 7);
	g_object_add_weak_pointer (G_OBJECT (owned), (gpointer *) &weak);
	g_assert_true (g_mime_gpgme_data_new (owned, &data, &error));
	g_assert_no_error (error);
	g_object_unref (owned);
	g_assert_nonnull (weak);
	{
		char buffer[7];
		g_assert_cmpint (gpgme_data_read (data, buffer, sizeof (buffer)), ==, 7);
		g_assert_cmpmem (buffer, sizeof (buffer), "adapter", 7);
	}
	gpgme_data_release (data);
	g_assert_null (weak);

	/* Low-level operations honor, rather than replace, native output settings. */
	for (armor = 0; armor < 2; armor++) {
		for (textmode = 0; textmode < 2; textmode++) {
			GMimeStream *probe = g_mime_stream_mem_new ();
			gpgme_sign_result_t probe_result = NULL;
			GByteArray *bytes;

			gpgme_set_armor (ctx, armor);
			gpgme_set_textmode (ctx, textmode);
			g_mime_stream_reset (input);
			g_assert_true (g_mime_gpgme_sign (ctx, GPGME_SIG_MODE_DETACH,
							input, probe, &probe_result, &error));
			g_assert_no_error (error);
			g_assert_nonnull (probe_result->signatures);
			g_assert_cmpuint (probe_result->signatures->sig_class, ==, textmode);
			bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (probe));
			g_assert_cmpint (bytes->len > 10 && !memcmp (bytes->data, "-----BEGIN", 10), ==, armor);
			g_assert_cmpint (gpgme_get_armor (ctx), ==, armor);
			g_assert_cmpint (gpgme_get_textmode (ctx), ==, textmode);
			gpgme_result_unref (probe_result);
			g_object_unref (probe);
		}
	}
	gpgme_set_armor (ctx, 0);
	gpgme_set_textmode (ctx, 0);
	g_mime_stream_reset (input);
	g_assert_true (g_mime_gpgme_sign (ctx, GPGME_SIG_MODE_DETACH,
					input, signature, &signed_result, &error));
	g_assert_no_error (error);
	g_assert_nonnull (signed_result->signatures);
	signature_fingerprint = g_strdup (signed_result->signatures->fpr);
	/* The API already returns a retained result; callers may take more refs. */
	gpgme_result_ref (signed_result);
	gpgme_result_unref (signed_result);
	g_mime_stream_reset (input);
	g_mime_stream_reset (signature);
	g_assert_true (g_mime_gpgme_verify (ctx, input, signature, NULL, &verified, &error));
	g_assert_no_error (error);
	g_assert_false (get_sig_status (verified) & GPGME_SIGSUM_RED);
	g_assert_null (verified->signatures->next);

	g_mime_stream_reset (signature);
	g_assert_true (g_mime_gpgme_verify (ctx, bad, signature, NULL, &invalid, &error));
	g_assert_no_error (error);
	g_assert_true (get_sig_status (invalid) & GPGME_SIGSUM_RED);
	g_assert_cmpuint (invalid->signatures->status, !=, 0);
	gpgme_result_unref (invalid);
	invalid = NULL;

	g_mime_stream_reset (input);
	g_assert_true (g_mime_gpgme_encrypt (ctx, TRUE, GPGME_ENCRYPT_ALWAYS_TRUST,
					   recipients, input, encrypted, &encrypted_result, &encrypted_sign, &error));
	g_assert_no_error (error);
	g_mime_stream_reset (encrypted);
	g_assert_true (g_mime_gpgme_decrypt (ctx, TRUE, encrypted, output,
					   &decrypted_result, &decrypted_verify, &error));
	g_assert_no_error (error);
	g_assert_false (get_sig_status (decrypted_verify) & GPGME_SIGSUM_RED);

	/* Backend failure may return retained diagnostics, never authentication. */
	g_mime_stream_reset (bad);
	invalid = (gpgme_verify_result_t) GINT_TO_POINTER (1);
	g_assert_false (g_mime_gpgme_verify (ctx, bad, NULL, output, &invalid, &error));
	g_assert_nonnull (error);
	g_assert_true (get_sig_status (invalid) & GPGME_SIGSUM_RED);
	g_clear_pointer (&invalid, gpgme_result_unref);
	g_clear_error (&error);
	g_assert_cmpint (gpgme_get_armor (ctx), ==, 0);
	g_assert_cmpint (gpgme_get_textmode (ctx), ==, 0);
	g_assert_cmpint (gpgme_get_offline (ctx), ==, 1);
	invalid = (gpgme_verify_result_t) GINT_TO_POINTER (1);
	g_assert_false (g_mime_gpgme_verify (ctx, input, NULL, NULL, &invalid, &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION);
	g_assert_null (invalid);
	g_clear_error (&error);
	g_assert_cmpuint (gpgme_signers_count (ctx), ==, 1);
	signer = gpgme_signers_enum (ctx, 0);
	g_assert_cmpstr (signer->subkeys->fpr, ==, key->subkeys->fpr);
	gpgme_key_unref (signer);

	/* GMime borrows ctx: it remains usable for direct native operations. */
	g_assert_cmpuint (gpgme_op_keylist_start (ctx, fingerprint, 0), ==, 0);
	g_assert_cmpuint (gpgme_op_keylist_end (ctx), ==, 0);
	gpgme_release (ctx);
	g_assert_cmpstr (signed_result->signatures->fpr, ==, signature_fingerprint);
	g_assert_cmpstr (verified->signatures->fpr, ==, signature_fingerprint);
	g_assert_cmpstr (encrypted_sign->signatures->fpr, ==, signature_fingerprint);
	g_assert_false (get_sig_status (decrypted_verify) & GPGME_SIGSUM_RED);
	g_assert_null (encrypted_result->invalid_recipients);
	g_assert_nonnull (decrypted_result->recipients);
	g_assert_cmpstr (key->subkeys->fpr, ==, fingerprint);
	gpgme_result_unref (signed_result);
	gpgme_result_unref (verified);
	gpgme_result_unref (encrypted_result);
	gpgme_result_unref (encrypted_sign);
	gpgme_result_unref (decrypted_result);
	gpgme_result_unref (decrypted_verify);
	gpgme_key_unref (key);
	g_free (signature_fingerprint);
	g_object_unref (bad);
	g_object_unref (output);
	g_object_unref (encrypted);
	g_object_unref (signature);
	g_object_unref (input);
}

static void
test_export (gpgme_ctx_t ctx, const char *path)
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

#endif

int main (int argc, char **argv)
{
#ifdef ENABLE_CRYPTO
	const char *datadir = "data/pgp";
	GMimeStream *istream, *ostream;
	GMimeFilterOpenPGP *filter;
	gpgme_ctx_t ctx;
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
	
	ctx = test_crypto_context (GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (ctx, 1);

	
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

	key = g_build_filename (datadir, "gmime.gpg.pub", NULL);
	testsuite_check ("GPGME OpenPGP::export");
	try {
		test_export (ctx, key);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME OpenPGP::export failed: %s", ex->message);
	} finally;
	
	g_free (key);
	
	istream = g_mime_stream_mem_new ();
	ostream = g_mime_stream_mem_new ();
	
	g_mime_stream_write_string (istream, "this is some cleartext\r\n");
	g_mime_stream_reset (istream);
	
	what = "GPGME OpenPGP::sign";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME OpenPGP::verify";
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
	
	what = "GPGME OpenPGP::sign (detached)";
	testsuite_check ("%s", what);
	try {
		test_sign (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME OpenPGP::verify (detached)";
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
	
	what = "GPGME OpenPGP::encrypt";
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, FALSE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME OpenPGP::decrypt";
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
	
	what = "GPGME OpenPGP::encrypt+sign";
	testsuite_check ("%s", what);
	try {
		test_encrypt (ctx, TRUE, istream, ostream);
		testsuite_check_passed ();
		
		what = "GPGME OpenPGP::decrypt+verify";
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
		test_gpgme_handles ();
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GPGME backend handles: %s", ex->message);
	} finally;

	gpgme_release (ctx);

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
