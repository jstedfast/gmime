/* Shared native GPGME fixture helpers; no crypto operations are emulated. */
#ifndef TEST_CRYPTO_H
#define TEST_CRYPTO_H

#include <stdio.h>
#include <gmime/gmime-gpgme.h>

static inline gpgme_error_t
test_passphrase (void *hook, const char *uid_hint, const char *info, int bad, int fd)
{
	return gpgme_io_writen (fd, "no.secret\n", 10) < 0 ?
		gpgme_error_from_syserror () : 0;
}

static inline gpgme_ctx_t
test_crypto_context (gpgme_protocol_t protocol)
{
	gpgme_ctx_t ctx = NULL;

	gpgme_check_version (NULL);
	g_assert_cmpuint (gpgme_new (&ctx), ==, 0);
	g_assert_cmpuint (gpgme_set_protocol (ctx, protocol), ==, 0);
	gpgme_set_offline (ctx, 1);
	gpgme_set_passphrase_cb (ctx, test_passphrase, NULL);
	g_assert_cmpuint (gpgme_set_pinentry_mode (ctx, GPGME_PINENTRY_MODE_LOOPBACK), ==, 0);
	return ctx;
}

static inline gpgme_key_t
test_crypto_key (gpgme_ctx_t ctx, const char *userid, gboolean secret)
{
	gpgme_key_t key = NULL;

	g_assert_cmpuint (gpgme_get_key (ctx, userid, &key, secret), ==, 0);
	g_assert_nonnull (key);
	return key;
}

static inline void
test_crypto_signer (gpgme_ctx_t ctx, const char *userid)
{
	gpgme_key_t key = test_crypto_key (ctx, userid, TRUE);

	g_assert_cmpuint (gpgme_signers_add (ctx, key), ==, 0);
	gpgme_key_unref (key);
}

static inline gpgme_sigsum_t
get_sig_status (gpgme_verify_result_t result)
{
	gpgme_sigsum_t status = 0;
	gpgme_signature_t sig;

	if (!result || !result->signatures)
		return GPGME_SIGSUM_RED;
	for (sig = result->signatures; sig; sig = sig->next) {
		status |= sig->summary;
		/* A nonzero native status must never count as authentication. */
		if (sig->status)
			status |= GPGME_SIGSUM_RED;
	}
	return status;
}

static inline void
print_verify_results (gpgme_verify_result_t result)
{
	gpgme_signature_t sig;

	for (sig = result ? result->signatures : NULL; sig; sig = sig->next)
		fprintf (stdout, "Signature %s: %s (summary 0x%x, validity %u)\n",
			 sig->fpr ? sig->fpr : "(unknown)", gpgme_strerror (sig->status),
			 sig->summary, sig->validity);
}

static inline void
test_crypto_import (gpgme_ctx_t ctx, GMimeStream *stream, GError **error)
{
	gpgme_data_t data;
	gpgme_error_t status;

	if (!g_mime_gpgme_data_new (stream, &data, error))
		return;
	status = gpgme_op_import (ctx, data);
	gpgme_data_release (data);
	if (status)
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_GENERAL, gpgme_strerror (status));
}

static inline void
test_crypto_export (gpgme_ctx_t ctx, const char **keys, GMimeStream *stream, GError **error)
{
	gpgme_data_t data;
	gpgme_error_t status;

	if (!g_mime_gpgme_data_new (stream, &data, error))
		return;
	status = gpgme_op_export_ext (ctx, keys, 0, data);
	gpgme_data_release (data);
	if (status)
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_GENERAL, gpgme_strerror (status));
}

#endif
