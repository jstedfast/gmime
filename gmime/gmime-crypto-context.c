/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2010 Jeffrey Stedfast
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

static GMimeCryptoHash crypto_hash_id (GMimeCryptoContext *ctx, const char *hash);

static const char *crypto_hash_name (GMimeCryptoContext *ctx, GMimeCryptoHash hash);

static const char *crypto_get_signature_protocol (GMimeCryptoContext *ctx);

static const char *crypto_get_encryption_protocol (GMimeCryptoContext *ctx);

static const char *crypto_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static int crypto_sign (GMimeCryptoContext *ctx, const char *userid,
			GMimeCryptoHash hash, GMimeStream *istream,
			GMimeStream *ostream, GError **err);
	
static GMimeSignatureValidity *crypto_verify (GMimeCryptoContext *ctx, GMimeCryptoHash hash,
					      GMimeStream *istream, GMimeStream *sigstream,
					      GError **err);
	
static int crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign,
			   const char *userid, GMimeCryptoHash hash,
			   GPtrArray *recipients, GMimeStream *istream,
			   GMimeStream *ostream, GError **err);

static GMimeDecryptionResult *crypto_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
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
	
	klass->hash_id = crypto_hash_id;
	klass->hash_name = crypto_hash_name;
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
	GMimeCryptoContext *ctx = (GMimeCryptoContext *) object;
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeCryptoHash
crypto_hash_id (GMimeCryptoContext *ctx, const char *hash)
{
	return GMIME_CRYPTO_HASH_DEFAULT;
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
 * g_mime_crypto_context_hash_id:
 * @ctx: a #GMimeCryptoContext
 * @hash: hash name
 *
 * Gets the hash id based on the hash name @hash.
 *
 * Returns: the equivalent hash id or #GMIME_CRYPTO_HASH_DEFAULT on fail.
 **/
GMimeCryptoHash
g_mime_crypto_context_hash_id (GMimeCryptoContext *ctx, const char *hash)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), GMIME_CRYPTO_HASH_DEFAULT);
	g_return_val_if_fail (hash != NULL, GMIME_CRYPTO_HASH_DEFAULT);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->hash_id (ctx, hash);
}


static const char *
crypto_hash_name (GMimeCryptoContext *ctx, GMimeCryptoHash hash)
{
	return NULL;
}


/**
 * g_mime_crypto_context_hash_name:
 * @ctx: a #GMimeCryptoContext
 * @hash: hash id
 *
 * Gets the hash name based on the hash id @hash.
 *
 * Returns: the equivalent hash name or %NULL on fail.
 **/
const char *
g_mime_crypto_context_hash_name (GMimeCryptoContext *ctx, GMimeCryptoHash hash)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->hash_name (ctx, hash);
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
crypto_sign (GMimeCryptoContext *ctx, const char *userid, GMimeCryptoHash hash,
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
 * @hash: digest algorithm to use
 * @istream: input stream
 * @ostream: output stream
 * @err: a #GError
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Returns: the #GMimeCryptoHash used on success (useful if @hash is
 * specified as #GMIME_CRYPTO_HASH_DEFAULT) or %-1 on fail.
 **/
int
g_mime_crypto_context_sign (GMimeCryptoContext *ctx, const char *userid, GMimeCryptoHash hash,
			    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->sign (ctx, userid, hash, istream, ostream, err);
}


static GMimeSignatureValidity *
crypto_verify (GMimeCryptoContext *ctx, GMimeCryptoHash hash, GMimeStream *istream,
	       GMimeStream *sigstream, GError **err)
{
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
		     "Verifying is not supported by this crypto context");
	
	return NULL;
}


/**
 * g_mime_crypto_context_verify:
 * @ctx: a #GMimeCryptoContext
 * @hash: digest algorithm used, if known
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @err: a #GError
 *
 * Verifies the signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Returns: a #GMimeSignatureValidity structure containing information
 * about the integrity of the input stream or %NULL on failure to
 * execute at all.
 **/
GMimeSignatureValidity *
g_mime_crypto_context_verify (GMimeCryptoContext *ctx, GMimeCryptoHash hash, GMimeStream *istream,
			      GMimeStream *sigstream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->verify (ctx, hash, istream, sigstream, err);
}


static int
crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeCryptoHash hash,
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
 * @hash: digest algorithm to use when signing
 * @recipients: an array of recipient key ids and/or email addresses
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
g_mime_crypto_context_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeCryptoHash hash,
			       GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->encrypt (ctx, sign, userid, hash, recipients, istream, ostream, err);
}


static GMimeDecryptionResult *
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
 * #GMimeDecryptionResult will have a non-%NULL #GMimeSignatureValidity which
 * will contain a list of signers, each with a #GMimeSignerStatus (among other
 * details about each signer).
 *
 * On success, the returned #GMimeDecryptionResult will contain a list of
 * recipient keys that the original encrypted stream was encrypted to.
 *
 * Returns: a #GMimeDecryptionResult on success or %NULL on error.
 **/
GMimeDecryptionResult *
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
 * @keys: an array of key ids
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


/**
 * g_mime_signer_new:
 * @status: A #GMimeSignerStatus
 *
 * Allocates an new #GMimeSigner with the designated @status. This
 * function is meant to be used in #GMimeCryptoContext subclasses when
 * allocating signers to add to a #GMimeSignatureValidity.
 *
 * Returns: a new #GMimeSigner with the designated @status.
 **/
GMimeSigner *
g_mime_signer_new (GMimeSignerStatus status)
{
	GMimeSigner *signer;
	
	signer = g_slice_new (GMimeSigner);
	signer->pubkey_algo = GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT;
	signer->hash_algo = GMIME_CRYPTO_HASH_DEFAULT;
	signer->status = status;
	signer->errors = GMIME_SIGNER_ERROR_NONE;
	signer->trust = GMIME_SIGNER_TRUST_NONE;
	signer->sig_created = (time_t) -1;
	signer->sig_expires = (time_t) -1;
	signer->key_created = (time_t) -1;
	signer->key_expires = (time_t) -1;
	signer->issuer_serial = NULL;
	signer->issuer_name = NULL;
	signer->fingerprint = NULL;
	signer->keyid = NULL;
	signer->email = NULL;
	signer->name = NULL;
	signer->next = NULL;
	
	return signer;
}


/**
 * g_mime_signer_free:
 * @signer: a #GMimeSigner
 *
 * Frees the singleton signer. Should NOT be used to free signers
 * returned from g_mime_signature_validity_get_signers().
 **/
void
g_mime_signer_free (GMimeSigner *signer)
{
	g_free (signer->issuer_serial);
	g_free (signer->issuer_name);
	g_free (signer->fingerprint);
	g_free (signer->keyid);
	g_free (signer->email);
	g_free (signer->name);
	
	g_slice_free (GMimeSigner, signer);
}


/**
 * g_mime_signer_next:
 * @signer: a #GMimeSigner
 *
 * Advance to the next signer.
 *
 * Returns: the next #GMimeSigner or %NULL when complete.
 **/
const GMimeSigner *
g_mime_signer_next (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->next;
}


/**
 * g_mime_signer_set_status:
 * @signer: a #GMimeSigner
 * @status: a #GMimeSignerStatus
 *
 * Set the status on the signer.
 **/
void
g_mime_signer_set_status (GMimeSigner *signer, GMimeSignerStatus status)
{
	g_return_if_fail (signer != NULL);
	
	signer->status = status;
}


/**
 * g_mime_signer_get_status:
 * @signer: a #GMimeSigner
 *
 * Get the signer status.
 *
 * Returns: the signer status.
 **/
GMimeSignerStatus
g_mime_signer_get_status (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, GMIME_SIGNER_STATUS_BAD);
	
	return signer->status;
}


/**
 * g_mime_signer_set_errors:
 * @signer: a #GMimeSigner
 * @error: a #GMimeSignerError
 *
 * Set the errors on the signer.
 **/
void
g_mime_signer_set_errors (GMimeSigner *signer, GMimeSignerError errors)
{
	g_return_if_fail (signer != NULL);
	
	signer->errors = errors;
}


/**
 * g_mime_signer_get_errors:
 * @signer: a #GMimeSigner
 *
 * Get the signer errors. If the #GMimeSignerStatus returned from
 * g_mime_signer_get_status() is not #GMIME_SIGNER_STATUS_GOOD, then
 * the errors may provide a clue as to why.
 *
 * Returns: the signer errors.
 **/
GMimeSignerError
g_mime_signer_get_errors (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, GMIME_SIGNER_ERROR_NONE);
	
	return signer->errors;
}


/**
 * g_mime_signer_set_trust:
 * @signer: a #GMimeSigner
 * @trust: a #GMimeSignerTrust
 *
 * Set the signer trust.
 **/
void
g_mime_signer_set_trust (GMimeSigner *signer, GMimeSignerTrust trust)
{
	g_return_if_fail (signer != NULL);
	
	signer->trust = trust;
}


/**
 * g_mime_signer_get_trust:
 * @signer: a #GMimeSigner
 *
 * Get the signer trust.
 *
 * Returns: the signer trust.
 **/
GMimeSignerTrust
g_mime_signer_get_trust (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, GMIME_SIGNER_TRUST_NONE);
	
	return signer->trust;
}


/**
 * g_mime_signer_set_sig_class:
 * @signer: a #GMimeSigner
 * @sig_class: signature class
 *
 * Set the signer's signature class.
 **/
void
g_mime_signer_set_sig_class (GMimeSigner *signer, int sig_class)
{
	g_return_if_fail (signer != NULL);
	
	signer->sig_class = (unsigned int) (sig_class & 0xff);
}


/**
 * g_mime_signer_get_sig_class:
 * @signer: a #GMimeSigner
 *
 * Get the signer's signature class.
 *
 * Returns: the signer's signature class.
 **/
int
g_mime_signer_get_sig_class (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, 0);
	
	return signer->sig_class;
}


/**
 * g_mime_signer_set_sig_version:
 * @signer: a #GMimeSigner
 * @sig_class: signature version
 *
 * Set the signer's signature version.
 **/
void
g_mime_signer_set_sig_version (GMimeSigner *signer, int version)
{
	g_return_if_fail (signer != NULL);
	
	signer->sig_ver = (unsigned int) (version & 0xff);
}


/**
 * g_mime_signer_get_sig_version:
 * @signer: a #GMimeSigner
 *
 * Get the signer's signature version.
 *
 * Returns: the signer's signature version.
 **/
int
g_mime_signer_get_sig_version (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, 0);
	
	return signer->sig_ver;
}


/**
 * g_mime_signer_set_pubkey_algo:
 * @signer: a #GMimeSigner
 * @pubkey_algo: a #GMimeCryptoPubKeyAlgo
 *
 * Set the public-key algorithm used by the signer.
 **/
void
g_mime_signer_set_pubkey_algo (GMimeSigner *signer, GMimeCryptoPubKeyAlgo pubkey_algo)
{
	g_return_if_fail (signer != NULL);
	
	signer->pubkey_algo = pubkey_algo;
}


/**
 * g_mime_signer_get_pubkey_algo:
 * @signer: a #GMimeSigner
 *
 * Get the public-key algorithm used by the signer.
 *
 * Returns: the public-key algorithm used by the signer.
 **/
GMimeCryptoPubKeyAlgo
g_mime_signer_get_pubkey_algo (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT);
	
	return signer->pubkey_algo;
}


/**
 * g_mime_signer_set_hash_algo:
 * @signer: a #GMimeSigner
 * @hash: a #GMimeCryptoHash
 *
 * Set the hash algorithm used by the signer.
 **/
void
g_mime_signer_set_hash_algo (GMimeSigner *signer, GMimeCryptoHash hash)
{
	g_return_if_fail (signer != NULL);
	
	signer->hash_algo = hash;
}


/**
 * g_mime_signer_get_hash_algo:
 * @signer: a #GMimeSigner
 *
 * Get the hash algorithm used by the signer.
 *
 * Returns: the hash algorithm used by the signer.
 **/
GMimeCryptoHash
g_mime_signer_get_hash_algo (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, GMIME_CRYPTO_HASH_DEFAULT);
	
	return signer->hash_algo;
}


/**
 * g_mime_signer_set_issuer_serial:
 * @signer: a #GMimeSigner
 * @issuer_serial: signer's issuer serial
 *
 * Set the signer's issuer serial.
 **/
void
g_mime_signer_set_issuer_serial (GMimeSigner *signer, const char *issuer_serial)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->issuer_serial);
	signer->issuer_serial = g_strdup (issuer_serial);
}


/**
 * g_mime_signer_get_issuer_serial:
 * @signer: a #GMimeSigner
 *
 * Get the signer's issuer serial.
 *
 * Returns: the signer's issuer serial.
 **/
const char *
g_mime_signer_get_issuer_serial (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->issuer_serial;
}


/**
 * g_mime_signer_set_issuer_name:
 * @signer: a #GMimeSigner
 * @issuer_name: signer's issuer name
 *
 * Set the signer's issuer name.
 **/
void
g_mime_signer_set_issuer_name (GMimeSigner *signer, const char *issuer_name)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->issuer_name);
	signer->issuer_name = g_strdup (issuer_name);
}


/**
 * g_mime_signer_get_issuer_name:
 * @signer: a #GMimeSigner
 *
 * Get the signer's issuer name.
 *
 * Returns: the signer's issuer name.
 **/
const char *
g_mime_signer_get_issuer_name (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->issuer_name;
}


/**
 * g_mime_signer_set_fingerprint:
 * @signer: a #GMimeSigner
 * @fingerprint: fingerprint string
 *
 * Set the signer's key fingerprint.
 **/
void
g_mime_signer_set_fingerprint (GMimeSigner *signer, const char *fingerprint)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->fingerprint);
	signer->fingerprint = g_strdup (fingerprint);
}


/**
 * g_mime_signer_get_fingerprint:
 * @signer: a #GMimeSigner
 *
 * Get the signer's key fingerprint.
 *
 * Returns: the signer's key fingerprint.
 **/
const char *
g_mime_signer_get_fingerprint (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->fingerprint;
}


/**
 * g_mime_signer_set_key_id:
 * @signer: a #GMimeSigner
 * @key_id: key id
 *
 * Set the signer's key id.
 **/
void
g_mime_signer_set_key_id (GMimeSigner *signer, const char *key_id)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->keyid);
	signer->keyid = g_strdup (key_id);
}


/**
 * g_mime_signer_get_key_id:
 * @signer: a #GMimeSigner
 *
 * Get the signer's key id.
 *
 * Returns: the signer's key id.
 **/
const char *
g_mime_signer_get_key_id (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->keyid;
}


/**
 * g_mime_signer_set_email:
 * @signer: a #GMimeSigner
 * @email: signer's email
 *
 * Set the signer's email.
 **/
void
g_mime_signer_set_email (GMimeSigner *signer, const char *email)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->email);
	signer->email = g_strdup (email);
}


/**
 * g_mime_signer_get_email:
 * @signer: a #GMimeSigner
 *
 * Get the signer's email.
 *
 * Returns: the signer's email.
 **/
const char *
g_mime_signer_get_email (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->email;
}


/**
 * g_mime_signer_set_name:
 * @signer: a #GMimeSigner
 * @name: signer's name
 *
 * Set the signer's name.
 **/
void
g_mime_signer_set_name (GMimeSigner *signer, const char *name)
{
	g_return_if_fail (signer != NULL);
	
	g_free (signer->name);
	signer->name = g_strdup (name);
}


/**
 * g_mime_signer_get_name:
 * @signer: a #GMimeSigner
 *
 * Get the signer's name.
 *
 * Returns: the signer's name.
 **/
const char *
g_mime_signer_get_name (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->name;
}


/**
 * g_mime_signer_set_sig_created:
 * @signer: a #GMimeSigner
 * @created: creation date
 *
 * Set the creation date of the signer's signature.
 **/
void
g_mime_signer_set_sig_created (GMimeSigner *signer, time_t created)
{
	g_return_if_fail (signer != NULL);
	
	signer->sig_created = created;
}


/**
 * g_mime_signer_get_sig_created:
 * @signer: a #GMimeSigner
 *
 * Get the creation date of the signer's signature.
 *
 * Returns: the creation date of the signer's signature or %-1 if
 * unknown.
 **/
time_t
g_mime_signer_get_sig_created (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->sig_created;
}


/**
 * g_mime_signer_set_sig_expires:
 * @signer: a #GMimeSigner
 * @expires: expiration date
 *
 * Set the expiration date of the signer's signature.
 **/
void
g_mime_signer_set_sig_expires (GMimeSigner *signer, time_t expires)
{
	g_return_if_fail (signer != NULL);
	
	signer->sig_expires = expires;
}


/**
 * g_mime_signer_get_sig_expires:
 * @signer: a #GMimeSigner
 *
 * Get the expiration date of the signer's signature.
 *
 * Returns: the expiration date of the signer's signature or %-1 if
 * unknown.
 **/
time_t
g_mime_signer_get_sig_expires (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->sig_expires;
}


/**
 * g_mime_signer_set_key_created:
 * @signer: a #GMimeSigner
 * @created: creation date
 *
 * Set the creation date of the signer's key.
 **/
void
g_mime_signer_set_key_created (GMimeSigner *signer, time_t created)
{
	g_return_if_fail (signer != NULL);
	
	signer->key_created = created;
}


/**
 * g_mime_signer_get_key_created:
 * @signer: a #GMimeSigner
 *
 * Get the creation date of the signer's key.
 *
 * Returns: the creation date of the signer's key or %-1 if unknown.
 **/
time_t
g_mime_signer_get_key_created (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->key_created;
}


/**
 * g_mime_signer_set_key_expires:
 * @signer: a #GMimeSigner
 * @expires: expiration date
 *
 * Set the expiration date of the signer's key.
 **/
void
g_mime_signer_set_key_expires (GMimeSigner *signer, time_t expires)
{
	g_return_if_fail (signer != NULL);
	
	signer->key_expires = expires;
}


/**
 * g_mime_signer_get_key_expires:
 * @signer: a #GMimeSigner
 *
 * Get the expiration date of the signer's key.
 *
 * Returns: the expiration date of the signer's key or %-1 if unknown.
 **/
time_t
g_mime_signer_get_key_expires (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->key_expires;
}


/**
 * g_mime_signature_validity_new:
 *
 * Creates a new #GMimeSignatureValidity.
 *
 * Returns: a new #GMimeSignatureValidity.
 **/
GMimeSignatureValidity *
g_mime_signature_validity_new (void)
{
	GMimeSignatureValidity *validity;
	
	validity = g_slice_new (GMimeSignatureValidity);
	validity->signers = NULL;
	validity->details = NULL;
	
	return validity;
}


/**
 * g_mime_signature_validity_free:
 * @validity: a #GMimeSignatureValidity
 *
 * Frees the memory used by @validity back to the system.
 **/
void
g_mime_signature_validity_free (GMimeSignatureValidity *validity)
{
	GMimeSigner *signer, *next;
	
	if (validity == NULL)
		return;
	
	signer = validity->signers;
	while (signer != NULL) {
		next = signer->next;
		g_mime_signer_free (signer);
		signer = next;
	}
	
	g_free (validity->details);
	
	g_slice_free (GMimeSignatureValidity, validity);
}


/**
 * g_mime_signature_validity_get_details:
 * @validity: a #GMimeSignatureValidity
 *
 * Gets any user-readable status details.
 *
 * Returns: a user-readable string containing any status information.
 **/
const char *
g_mime_signature_validity_get_details (const GMimeSignatureValidity *validity)
{
	g_return_val_if_fail (validity != NULL, NULL);
	
	return validity->details;
}


/**
 * g_mime_signature_validity_set_details:
 * @validity: a #GMimeSignatureValidity
 * @details: details string
 *
 * Sets @details as the status details string on @validity.
 **/
void
g_mime_signature_validity_set_details (GMimeSignatureValidity *validity, const char *details)
{
	g_return_if_fail (validity != NULL);
	
	g_free (validity->details);
	validity->details = g_strdup (details);
}


/**
 * g_mime_signature_validity_get_signers:
 * @validity: a #GMimeSignatureValidity
 *
 * Gets the list of signers.
 *
 * Returns: a #GMimeSigner list which contain further information such
 * as trust and crypto keys. These signers are part of the
 * #GMimeSignatureValidity and should NOT be freed individually.
 **/
const GMimeSigner *
g_mime_signature_validity_get_signers (const GMimeSignatureValidity *validity)
{
	g_return_val_if_fail (validity != NULL, NULL);
	
	return validity->signers;
}


/**
 * g_mime_signature_validity_add_signer:
 * @validity: a #GMimeSignatureValidity
 * @signer: a #GMimeSigner
 *
 * Adds @signer to the list of signers on @validity. Once the signer
 * is added, it must NOT be freed.
 **/
void
g_mime_signature_validity_add_signer (GMimeSignatureValidity *validity, GMimeSigner *signer)
{
	GMimeSigner *s;
	
	g_return_if_fail (validity != NULL);
	g_return_if_fail (signer != NULL);
	
	if (validity->signers == NULL) {
		validity->signers = signer;
	} else {
		s = validity->signers;
		while (s->next != NULL)
			s = s->next;
		
		s->next = signer;
	}
}


/**
 * g_mime_crypto_recipient_new:
 *
 * Allocates an new #GMimeCryptoRecipient. This function is meant to be used
 * in #GMimeCryptoContext subclasses when allocating recipients to add to a
 * #GMimeDecryptionResult.
 *
 * Returns: a new #GMimeCryptoRecipient.
 **/
GMimeCryptoRecipient *
g_mime_crypto_recipient_new (void)
{
	GMimeCryptoRecipient *recipient;
	
	recipient = g_slice_new (GMimeCryptoRecipient);
	recipient->pubkey_algo = GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT;
	recipient->keyid = NULL;
	recipient->next = NULL;
	
	return recipient;
}


/**
 * g_mime_crypto_recipient_free:
 * @recipient: a #GMimeCryptoRecipient
 *
 * Frees the singleton recipient. Should NOT be used to free recipients
 * returned from g_mime_signature_validity_get_recipients().
 **/
void
g_mime_crypto_recipient_free (GMimeCryptoRecipient *recipient)
{
	g_free (recipient->keyid);
	
	g_slice_free (GMimeCryptoRecipient, recipient);
}


/**
 * g_mime_crypto_recipient_next:
 * @recipient: a #GMimeCryptoRecipient
 *
 * Advance to the next recipient.
 *
 * Returns: the next #GMimeCryptoRecipient or %NULL when complete.
 **/
const GMimeCryptoRecipient *
g_mime_crypto_recipient_next (const GMimeCryptoRecipient *recipient)
{
	g_return_val_if_fail (recipient != NULL, NULL);
	
	return recipient->next;
}


/**
 * g_mime_crypto_recipient_set_pubkey_algo:
 * @recipient: a #GMimeCryptoRecipient
 * @pubkey_algo: a #GMimeCryptoPubKeyAlgo
 *
 * Set the public-key algorithm used by the recipient.
 **/
void
g_mime_crypto_recipient_set_pubkey_algo (GMimeCryptoRecipient *recipient, GMimeCryptoPubKeyAlgo pubkey_algo)
{
	g_return_if_fail (recipient != NULL);
	
	recipient->pubkey_algo = pubkey_algo;
}


/**
 * g_mime_crypto_recipient_get_pubkey_algo:
 * @recipient: a #GMimeCryptoRecipient
 *
 * Get the public-key algorithm used by the recipient.
 *
 * Returns: the public-key algorithm used by the recipient.
 **/
GMimeCryptoPubKeyAlgo
g_mime_crypto_recipient_get_pubkey_algo (const GMimeCryptoRecipient *recipient)
{
	g_return_val_if_fail (recipient != NULL, GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT);
	
	return recipient->pubkey_algo;
}


/**
 * g_mime_crypto_recipient_set_key_id:
 * @recipient: a #GMimeCryptoRecipient
 * @key_id: key id
 *
 * Set the recipient's key id.
 **/
void
g_mime_crypto_recipient_set_key_id (GMimeCryptoRecipient *recipient, const char *key_id)
{
	g_return_if_fail (recipient != NULL);
	
	g_free (recipient->keyid);
	recipient->keyid = g_strdup (key_id);
}


/**
 * g_mime_crypto_recipient_get_key_id:
 * @recipient: a #GMimeCryptoRecipient
 *
 * Get the recipient's key id.
 *
 * Returns: the recipient's key id.
 **/
const char *
g_mime_crypto_recipient_get_key_id (const GMimeCryptoRecipient *recipient)
{
	g_return_val_if_fail (recipient != NULL, NULL);
	
	return recipient->keyid;
}


/**
 * g_mime_decryption_result_new:
 *
 * Creates a new #GMimeDecryptionResult.
 *
 * Returns: a new #GMimeDecryptionResult.
 **/
GMimeDecryptionResult *
g_mime_decryption_result_new (void)
{
	GMimeDecryptionResult *result;
	
	result = g_slice_new (GMimeDecryptionResult);
	result->recipients = NULL;
	result->validity = NULL;
	
	return result;
}


/**
 * g_mime_decryption_result_free:
 * @result: a #GMimeDecryptionResult
 *
 * Frees the memory used by @result back to the system.
 **/
void
g_mime_decryption_result_free (GMimeDecryptionResult *result)
{
	GMimeCryptoRecipient *recipient, *next;
	
	if (result == NULL)
		return;
	
	recipient = result->recipients;
	while (recipient != NULL) {
		next = recipient->next;
		g_mime_crypto_recipient_free (recipient);
		recipient = next;
	}
	
	g_mime_signature_validity_free (result->validity);
	
	g_slice_free (GMimeDecryptionResult, result);
}


/**
 * g_mime_decryption_result_get_validity:
 * @result: a #GMimeDecryptionResult
 *
 * Gets the signature validity if the decrypted stream was also signed.
 *
 * Returns: a #GMimeSignatureValidity or %NULL if the stream was not signed.
 **/
const GMimeSignatureValidity *
g_mime_decryption_result_get_validity (const GMimeDecryptionResult *result)
{
	g_return_val_if_fail (result != NULL, NULL);
	
	return result->validity;
}


/**
 * g_mime_decryption_result_set_validity:
 * @result: a #GMimeDecryptionResult
 * @validity: a #GMimeSignatureValidity
 *
 * Sets @validity as the #GMimeDecryptionResult.
 **/
void
g_mime_decryption_result_set_validity (GMimeDecryptionResult *result, GMimeSignatureValidity *validity)
{
	g_return_if_fail (result != NULL);
	
	g_mime_signature_validity_free (result->validity);
	result->validity = validity;
}


/**
 * g_mime_decryption_result_get_recipients:
 * @result: signature result
 *
 * Gets the list of recipients.
 *
 * Returns: a #GMimeCryptoRecipient list which contain further information such
 * as trust and crypto keys. These recipients are part of the
 * #GMimeDecryptionResult and should NOT be freed individually.
 **/
const GMimeCryptoRecipient *
g_mime_decryption_result_get_recipients (const GMimeDecryptionResult *result)
{
	g_return_val_if_fail (result != NULL, NULL);
	
	return result->recipients;
}


/**
 * g_mime_decryption_result_add_recipient:
 * @result: a #GMimeDecryptionResult
 * @recipient: a #GMimeCryptoRecipient
 *
 * Adds @recipient to the list of recipients on @result. Once the recipient
 * is added, it must NOT be freed.
 **/
void
g_mime_decryption_result_add_recipient (GMimeDecryptionResult *result, GMimeCryptoRecipient *recipient)
{
	GMimeCryptoRecipient *r;
	
	g_return_if_fail (result != NULL);
	g_return_if_fail (recipient != NULL);
	
	if (result->recipients == NULL) {
		result->recipients = recipient;
	} else {
		r = result->recipients;
		while (r->next != NULL)
			r = r->next;
		
		r->next = recipient;
	}
}
