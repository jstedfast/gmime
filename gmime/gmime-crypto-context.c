/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-crypto-context.h"
#include "gmime-error.h"


/**
 * SECTION: gmime-crypto-context
 * @title: GMimeCryptoContext
 * @short_description: Encryption/signing contexts
 * @see_also:
 *
 * A #GMimeCryptoContext is used for encrypting, decrypting, signing
 * and verifying cryptographic signatures.
 **/


static void g_mime_crypto_context_class_init (GMimeCryptoContextClass *klass);
static void g_mime_crypto_context_init (GMimeCryptoContext *ctx, GMimeCryptoContextClass *klass);
static void g_mime_crypto_context_finalize (GObject *object);

static GMimeDigestAlgo crypto_digest_id (GMimeCryptoContext *ctx, const char *name);

static const char *crypto_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo );

static const char *crypto_get_signature_protocol (GMimeCryptoContext *ctx);

static const char *crypto_get_encryption_protocol (GMimeCryptoContext *ctx);

static const char *crypto_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static int crypto_sign (GMimeCryptoContext *ctx, const char *userid,
			GMimeDigestAlgo digest, GMimeStream *istream,
			GMimeStream *ostream, GError **err);
	
static GMimeSignatureList *crypto_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
					  GMimeStream *istream, GMimeStream *sigstream,
					  GError **err);
	
static int crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign,
			   const char *userid, GMimeDigestAlgo digest,
			   GPtrArray *recipients, GMimeStream *istream,
			   GMimeStream *ostream, GError **err);

static GMimeDecryptResult *crypto_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
					   GMimeStream *ostream, GError **err);

static int crypto_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream,
			       GError **err);

static int crypto_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
			       GMimeStream *ostream, GError **err);


static GObjectClass *parent_class = NULL;


GType
g_mime_crypto_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeCryptoContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_crypto_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeCryptoContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_crypto_context_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeCryptoContext", &info, 0);
	}
	
	return type;
}


static void
g_mime_crypto_context_class_init (GMimeCryptoContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_crypto_context_finalize;
	
	klass->digest_id = crypto_digest_id;
	klass->digest_name = crypto_digest_name;
	klass->sign = crypto_sign;
	klass->verify = crypto_verify;
	klass->encrypt = crypto_encrypt;
	klass->decrypt = crypto_decrypt;
	klass->import_keys = crypto_import_keys;
	klass->export_keys = crypto_export_keys;
	klass->get_signature_protocol = crypto_get_signature_protocol;
	klass->get_encryption_protocol = crypto_get_encryption_protocol;
	klass->get_key_exchange_protocol = crypto_get_key_exchange_protocol;
}

static void
g_mime_crypto_context_init (GMimeCryptoContext *ctx, GMimeCryptoContextClass *klass)
{
	ctx->request_passwd = NULL;
}

static void
g_mime_crypto_context_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_crypto_context_set_request_password:
 * @ctx: a #GMimeCryptoContext
 * @request_passwd: a callback function for requesting a password
 *
 * Sets the function used by the @ctx for requesting a password from
 * the user.
 **/
void
g_mime_crypto_context_set_request_password (GMimeCryptoContext *ctx, GMimePasswordRequestFunc request_passwd)
{
	g_return_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx));
	
	ctx->request_passwd = request_passwd;
}


static GMimeDigestAlgo
crypto_digest_id (GMimeCryptoContext *ctx, const char *name)
{
	return GMIME_DIGEST_ALGO_DEFAULT;
}


/**
 * g_mime_crypto_context_get_request_password:
 * @ctx: a #GMimeCryptoContext
 *
 * Gets the function used by the @ctx for requesting a password from
 * the user.
 *
 * Returns: a #GMimePasswordRequestFunc or %NULL if not set.
 **/
GMimePasswordRequestFunc
g_mime_crypto_context_get_request_password (GMimeCryptoContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return ctx->request_passwd;
}


/**
 * g_mime_crypto_context_digest_id:
 * @ctx: a #GMimeCryptoContext
 * @name: digest name
 *
 * Gets the digest id based on the digest name.
 *
 * Returns: the equivalent digest id or #GMIME_DIGEST_ALGO_DEFAULT on fail.
 **/
GMimeDigestAlgo
g_mime_crypto_context_digest_id (GMimeCryptoContext *ctx, const char *name)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), GMIME_DIGEST_ALGO_DEFAULT);
	g_return_val_if_fail (name != NULL, GMIME_DIGEST_ALGO_DEFAULT);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->digest_id (ctx, name);
}


static const char *
crypto_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest)
{
	return NULL;
}


/**
 * g_mime_crypto_context_digest_name:
 * @ctx: a #GMimeCryptoContext
 * @digest: digest id
 *
 * Gets the digest name based on the digest id @digest.
 *
 * Returns: the equivalent digest name or %NULL on fail.
 **/
const char *
g_mime_crypto_context_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->digest_name (ctx, digest);
}


static const char *
crypto_get_signature_protocol (GMimeCryptoContext *ctx)
{
	return NULL;
}


/**
 * g_mime_crypto_context_get_signature_protocol:
 * @ctx: a #GMimeCryptoContext
 *
 * Gets the signature protocol for the crypto context.
 *
 * Returns: the signature protocol or %NULL if not supported.
 **/
const char *
g_mime_crypto_context_get_signature_protocol (GMimeCryptoContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->get_signature_protocol (ctx);
}


static const char *
crypto_get_encryption_protocol (GMimeCryptoContext *ctx)
{
	return NULL;
}


/**
 * g_mime_crypto_context_get_encryption_protocol:
 * @ctx: a #GMimeCryptoContext
 *
 * Gets the encryption protocol for the crypto context.
 *
 * Returns: the encryption protocol or %NULL if not supported.
 **/
const char *
g_mime_crypto_context_get_encryption_protocol (GMimeCryptoContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->get_encryption_protocol (ctx);
}


static const char *
crypto_get_key_exchange_protocol (GMimeCryptoContext *ctx)
{
	return NULL;
}


/**
 * g_mime_crypto_context_get_key_exchange_protocol:
 * @ctx: a #GMimeCryptoContext
 *
 * Gets the key exchange protocol for the crypto context.
 *
 * Returns: the key exchange protocol or %NULL if not supported.
 **/
const char *
g_mime_crypto_context_get_key_exchange_protocol (GMimeCryptoContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->get_key_exchange_protocol (ctx);
}


static int
crypto_sign (GMimeCryptoContext *ctx, const char *userid, GMimeDigestAlgo digest,
	     GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "Signing is not supported by this crypto context");
	
	return -1;
}


/**
 * g_mime_crypto_context_sign:
 * @ctx: a #GMimeCryptoContext
 * @userid: private key to use to sign the stream
 * @digest: digest algorithm to use
 * @istream: input stream
 * @ostream: output stream
 * @err: a #GError
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Returns: the #GMimeDigestAlgo used on success (useful if @digest is
 * specified as #GMIME_DIGEST_ALGO_DEFAULT) or %-1 on fail.
 **/
int
g_mime_crypto_context_sign (GMimeCryptoContext *ctx, const char *userid, GMimeDigestAlgo digest,
			    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->sign (ctx, userid, digest, istream, ostream, err);
}


static GMimeSignatureList *
crypto_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest, GMimeStream *istream,
	       GMimeStream *sigstream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "Verifying is not supported by this crypto context");
	
	return NULL;
}


/**
 * g_mime_crypto_context_verify:
 * @ctx: a #GMimeCryptoContext
 * @digest: digest algorithm used, if known
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @err: a #GError
 *
 * Verifies the signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Returns: (transfer full): a #GMimeSignatureList object containing
 * the status of each signature or %NULL on error.
 **/
GMimeSignatureList *
g_mime_crypto_context_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest, GMimeStream *istream,
			      GMimeStream *sigstream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->verify (ctx, digest, istream, sigstream, err);
}


static int
crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeDigestAlgo digest,
		GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "Encryption is not supported by this crypto context");
	
	return -1;
}


/**
 * g_mime_crypto_context_encrypt:
 * @ctx: a #GMimeCryptoContext
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @digest: digest algorithm to use when signing
 * @recipients: (element-type utf8): an array of recipient key ids
 *   and/or email addresses
 * @istream: cleartext input stream
 * @ostream: ciphertext output stream
 * @err: a #GError
 *
 * Encrypts (and optionally signs) the cleartext input stream and
 * writes the resulting ciphertext to the output stream.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_crypto_context_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeDigestAlgo digest,
			       GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->encrypt (ctx, sign, userid, digest, recipients, istream, ostream, err);
}


static GMimeDecryptResult *
crypto_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
		GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "Decryption is not supported by this crypto context");
	
	return NULL;
}


/**
 * g_mime_crypto_context_decrypt:
 * @ctx: a #GMimeCryptoContext
 * @istream: input/ciphertext stream
 * @ostream: output/cleartext stream
 * @err: a #GError
 *
 * Decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * If the encrypted input stream was also signed, the returned
 * #GMimeDecryptResult will have a non-%NULL list of signatures, each with a
 * #GMimeSignatureStatus (among other details about each signature).
 *
 * On success, the returned #GMimeDecryptResult will contain a list of
 * certificates, one for each recipient, that the original encrypted stream
 * was encrypted to.
 *
 * Note: It *may* be possible to maliciously design an encrypted stream such
 * that recursively decrypting it will result in ane endless loop, causing
 * a denial of service attack on your application.
 *
 * Returns: (transfer full): a #GMimeDecryptResult on success or %NULL
 * on error.
 **/
GMimeDecryptResult *
g_mime_crypto_context_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
			       GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->decrypt (ctx, istream, ostream, err);
}


static int
crypto_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "You may not import keys with this crypto");
	
	return -1;
}


/**
 * g_mime_crypto_context_import_keys:
 * @ctx: a #GMimeCryptoContext
 * @istream: input stream (containing keys)
 * @err: a #GError
 *
 * Imports a stream of keys/certificates contained within @istream
 * into the key/certificate database controlled by @ctx.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_crypto_context_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->import_keys (ctx, istream, err);
}


static int
crypto_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
		    GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "You may not export keys with this crypto");
	
	return -1;
}


/**
 * g_mime_crypto_context_export_keys:
 * @ctx: a #GMimeCryptoContext
 * @keys: (element-type utf8): an array of key ids
 * @ostream: output stream
 * @err: a #GError
 *
 * Exports the keys/certificates in @keys to the stream @ostream from
 * the key/certificate database controlled by @ctx.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_crypto_context_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
				   GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	g_return_val_if_fail (keys != NULL, -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->export_keys (ctx, keys, ostream, err);
}


static void g_mime_decrypt_result_class_init (GMimeDecryptResultClass *klass);
static void g_mime_decrypt_result_init (GMimeDecryptResult *cert, GMimeDecryptResultClass *klass);
static void g_mime_decrypt_result_finalize (GObject *object);

static GObjectClass *result_parent_class = NULL;


GType
g_mime_decrypt_result_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeDecryptResultClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_decrypt_result_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeDecryptResult),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_decrypt_result_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeDecryptResult", &info, 0);
	}
	
	return type;
}

static void
g_mime_decrypt_result_class_init (GMimeDecryptResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	result_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_decrypt_result_finalize;
}

static void
g_mime_decrypt_result_init (GMimeDecryptResult *result, GMimeDecryptResultClass *klass)
{
	result->cipher = GMIME_CIPHER_ALGO_DEFAULT;
	result->mdc = GMIME_DIGEST_ALGO_DEFAULT;
	result->recipients = NULL;
	result->signatures = NULL;
}

static void
g_mime_decrypt_result_finalize (GObject *object)
{
	GMimeDecryptResult *result = (GMimeDecryptResult *) object;
	
	if (result->recipients)
		g_object_unref (result->recipients);
	
	if (result->signatures)
		g_object_unref (result->signatures);
	
	G_OBJECT_CLASS (result_parent_class)->finalize (object);
}


/**
 * g_mime_decrypt_result_new:
 *
 * Creates a new #GMimeDecryptResult object.
 * 
 * Returns: a new #GMimeDecryptResult object.
 **/
GMimeDecryptResult *
g_mime_decrypt_result_new (void)
{
	return g_object_newv (GMIME_TYPE_DECRYPT_RESULT, 0, NULL);
}


/**
 * g_mime_decrypt_result_set_recipients:
 * @result: A #GMimeDecryptResult
 * @recipients: A #GMimeCertificateList
 *
 * Sets the list of certificates that the stream had been encrypted to.
 **/
void
g_mime_decrypt_result_set_recipients (GMimeDecryptResult *result, GMimeCertificateList *recipients)
{
	g_return_if_fail (GMIME_IS_DECRYPT_RESULT (result));
	g_return_if_fail (GMIME_IS_CERTIFICATE_LIST (recipients));
	
	if (result->recipients == recipients)
		return;
	
	if (result->recipients)
		g_object_unref (result->recipients);
	
	if (recipients)
		g_object_ref (recipients);
	
	result->recipients = recipients;
}


/**
 * g_mime_decrypt_result_get_recipients:
 * @result: A #GMimeDecryptResult
 *
 * Gets the list of certificates that the stream had been encrypted to.
 *
 * Returns: (transfer none): a #GMimeCertificateList.
 **/
GMimeCertificateList *
g_mime_decrypt_result_get_recipients (GMimeDecryptResult *result)
{
	g_return_val_if_fail (GMIME_IS_DECRYPT_RESULT (result), NULL);
	
	return result->recipients;
}


/**
 * g_mime_decrypt_result_set_signatures:
 * @result: A #GMimeDecryptResult
 * @signatures: A #GMimeSignatureList
 *
 * Sets the list of signatures.
 **/
void
g_mime_decrypt_result_set_signatures (GMimeDecryptResult *result, GMimeSignatureList *signatures)
{
	g_return_if_fail (GMIME_IS_DECRYPT_RESULT (result));
	g_return_if_fail (GMIME_IS_SIGNATURE_LIST (signatures));
	
	if (result->signatures == signatures)
		return;
	
	if (result->signatures)
		g_object_unref (result->signatures);
	
	if (signatures)
		g_object_ref (signatures);
	
	result->signatures = signatures;
}


/**
 * g_mime_decrypt_result_get_signatures:
 * @result: A #GMimeDecryptResult
 *
 * Gets a list of signatures if the encrypted stream had also been signed.
 *
 * Returns: (transfer none): a #GMimeSignatureList or %NULL if the
 * stream was not signed.
 **/
GMimeSignatureList *
g_mime_decrypt_result_get_signatures (GMimeDecryptResult *result)
{
	g_return_val_if_fail (GMIME_IS_DECRYPT_RESULT (result), NULL);
	
	return result->signatures;
}


/**
 * g_mime_decrypt_result_set_cipher:
 * @result: a #GMimeDecryptResult
 * @cipher: a #GMimeCipherAlgo
 *
 * Set the cipher algorithm used.
 **/
void
g_mime_decrypt_result_set_cipher (GMimeDecryptResult *result, GMimeCipherAlgo cipher)
{
	g_return_if_fail (GMIME_IS_DECRYPT_RESULT (result));
	
	result->cipher = cipher;
}


/**
 * g_mime_decrypt_result_get_cipher:
 * @result: a #GMimeDecryptResult
 *
 * Get the cipher algorithm used.
 *
 * Returns: the cipher algorithm used.
 **/
GMimeCipherAlgo
g_mime_decrypt_result_get_cipher (GMimeDecryptResult *result)
{
	g_return_val_if_fail (GMIME_IS_DECRYPT_RESULT (result), GMIME_CIPHER_ALGO_DEFAULT);
	
	return result->cipher;
}


/**
 * g_mime_decrypt_result_set_mdc:
 * @result: a #GMimeDecryptResult
 * @mdc: a #GMimeDigestAlgo
 *
 * Set the mdc digest algorithm used.
 **/
void
g_mime_decrypt_result_set_mdc (GMimeDecryptResult *result, GMimeDigestAlgo mdc)
{
	g_return_if_fail (GMIME_IS_DECRYPT_RESULT (result));
	
	result->mdc = mdc;
}


/**
 * g_mime_decrypt_result_get_mdc:
 * @result: a #GMimeDecryptResult
 *
 * Get the mdc digest algorithm used.
 *
 * Returns: the mdc digest algorithm used.
 **/
GMimeDigestAlgo
g_mime_decryption_result_get_mdc (GMimeDecryptResult *result)
{
	g_return_val_if_fail (GMIME_IS_DECRYPT_RESULT (result), GMIME_DIGEST_ALGO_DEFAULT);
	
	return result->mdc;
}
