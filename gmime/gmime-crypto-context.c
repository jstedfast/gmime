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

static GMimeSignatureValidity *crypto_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
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


static GMimeSignatureValidity *
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
 * #GMimeSignatureValidity will contain a list of signers, each with a
 * #GMimeSignerStatus (among other details).
 *
 * If the encrypted input text was not signed, then the
 * #GMimeSignatureValidity will not contain any signers.
 *
 * Returns: a #GMimeSignatureValidity on success or %NULL on error.
 **/
GMimeSignatureValidity *
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
 * g_mime_crypto_key_new:
 * @key_id: The key id or %NULL if unspecified.
 *
 * Allocates an new #GMimeCryptoKey with the designated @key_id. This
 * function is meant to be used in #GMimeCryptoContext subclasses when
 * creating keys to be used with #GMimeSigner.
 *
 * Returns: a new #GMimeCryptoKey.
 **/
static GMimeCryptoKey *
g_mime_crypto_key_new (const char *key_id)
{
	GMimeCryptoKey *key;
	
	key = g_slice_new (GMimeCryptoKey);
	key->pubkey_algo = GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT;
	key->created = (time_t) -1;
	key->expires = (time_t) -1;
	key->issuer_serial = NULL;
	key->issuer_name = NULL;
	key->fingerprint = NULL;
	key->keyid = NULL;
	key->email = NULL;
	key->name = NULL;
	
	return key;
}


/**
 * g_mime_crypto_key_free:
 * @key: a #GMimeCryptoKey
 *
 * Frees the crypto key.
 **/
static void
g_mime_crypto_key_free (GMimeCryptoKey *key)
{
	if (key != NULL) {
		g_free (key->issuer_serial);
		g_free (key->issuer_name);
		g_free (key->fingerprint);
		g_free (key->keyid);
		g_free (key->email);
		g_free (key->name);
		
		g_slice_free (GMimeCryptoKey, key);
	}
}


/**
 * g_mime_crypto_key_set_pubkey_algo:
 * @key: a #GMimeCryptoKey
 * @pubkey_algo: a #GMimeCryptoPubKeyAlgo
 *
 * Set the public-key algorithm associated with the key.
 **/
void
g_mime_crypto_key_set_pubkey_algo (GMimeCryptoKey *key, GMimeCryptoPubKeyAlgo pubkey_algo)
{
	g_return_if_fail (key != NULL);
	
	key->pubkey_algo = pubkey_algo;
}


/**
 * g_mime_crypto_key_get_pubkey_algo:
 * @key: a #GMimeCryptoKey
 *
 * Get the public-key algorithm associated with @key.
 *
 * Returns: the public-key algorithm associated with the key.
 **/
GMimeCryptoPubKeyAlgo
g_mime_crypto_key_get_pubkey_algo (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, GMIME_CRYPTO_PUBKEY_ALGO_DEFAULT);
	
	return key->pubkey_algo;
}


/**
 * g_mime_crypto_key_set_issuer_serial:
 * @key: a #GMimeCryptoKey
 * @issuer_serial: issuer serial
 *
 * Set the key's issuer serial.
 **/
void
g_mime_crypto_key_set_issuer_serial (GMimeCryptoKey *key, const char *issuer_serial)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->issuer_serial);
	key->issuer_serial = g_strdup (issuer_serial);
}


/**
 * g_mime_crypto_key_get_issuer_serial:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's issuer serial.
 *
 * Returns: the key's issuer serial.
 **/
const char *
g_mime_crypto_key_get_issuer_serial (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->issuer_serial;
}


/**
 * g_mime_crypto_key_set_issuer_name:
 * @key: a #GMimeCryptoKey
 * @issuer_name: issuer name
 *
 * Set the key's issuer name.
 **/
void
g_mime_crypto_key_set_issuer_name (GMimeCryptoKey *key, const char *issuer_name)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->issuer_name);
	key->issuer_name = g_strdup (issuer_name);
}


/**
 * g_mime_crypto_key_get_issuer_name:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's issuer name.
 *
 * Returns: the key's issuer name.
 **/
const char *
g_mime_crypto_key_get_issuer_name (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->issuer_name;
}


/**
 * g_mime_crypto_key_set_fingerprint:
 * @key: a #GMimeCryptoKey
 * @fingerprint: fingerprint string
 *
 * Set the key's fingerprint.
 **/
void
g_mime_crypto_key_set_fingerprint (GMimeCryptoKey *key, const char *fingerprint)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->fingerprint);
	key->fingerprint = g_strdup (fingerprint);
}


/**
 * g_mime_crypto_key_get_fingerprint:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's fingerprint.
 *
 * Returns: the key's fingerprint.
 **/
const char *
g_mime_crypto_key_get_fingerprint (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->fingerprint;
}


/**
 * g_mime_crypto_key_set_key_id:
 * @key: a #GMimeCryptoKey
 * @key_id: key id
 *
 * Set the key's id.
 **/
void
g_mime_crypto_key_set_key_id (GMimeCryptoKey *key, const char *key_id)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->keyid);
	key->keyid = g_strdup (key_id);
}


/**
 * g_mime_crypto_key_get_key_id:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's id.
 *
 * Returns: the key's id.
 **/
const char *
g_mime_crypto_key_get_key_id (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->keyid;
}


/**
 * g_mime_crypto_key_set_email:
 * @key: a #GMimeCryptoKey
 * @email: email address
 *
 * Set the key's email address.
 **/
void
g_mime_crypto_key_set_email (GMimeCryptoKey *key, const char *email)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->email);
	key->email = g_strdup (email);
}


/**
 * g_mime_crypto_key_get_email:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's email address.
 *
 * Returns: the key's email address.
 **/
const char *
g_mime_crypto_key_get_email (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->email;
}


/**
 * g_mime_crypto_key_set_name:
 * @key: a #GMimeCryptoKey
 * @name: key's name
 *
 * Set the key's name.
 **/
void
g_mime_crypto_key_set_name (GMimeCryptoKey *key, const char *name)
{
	g_return_if_fail (key != NULL);
	
	g_free (key->name);
	key->name = g_strdup (name);
}


/**
 * g_mime_crypto_key_get_name:
 * @key: a #GMimeCryptoKey
 *
 * Get the key's name.
 *
 * Returns: the key's name.
 **/
const char *
g_mime_crypto_key_get_name (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, NULL);
	
	return key->name;
}


/**
 * g_mime_crypto_key_set_creation_date:
 * @key: a #GMimeCryptoKey
 * @created: creation date
 *
 * Set the creation date of the key.
 **/
void
g_mime_crypto_key_set_creation_date (GMimeCryptoKey *key, time_t created)
{
	g_return_if_fail (key != NULL);
	
	key->created = created;
}


/**
 * g_mime_crypto_key_get_creation_date:
 * @key: a #GMimeCryptoKey
 *
 * Get the creation date of the key.
 *
 * Returns: the creation date of the key or %-1 if unknown.
 **/
time_t
g_mime_crypto_key_get_creation_date (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, (time_t) -1);
	
	return key->created;
}


/**
 * g_mime_crypto_key_set_expiration_date:
 * @key: a #GMimeCryptoKey
 * @expires: expiration date
 *
 * Set the expiration date of the key.
 **/
void
g_mime_crypto_key_set_expiration_date (GMimeCryptoKey *key, time_t expires)
{
	g_return_if_fail (key != NULL);
	
	key->expires = expires;
}


/**
 * g_mime_crypto_key_get_expiration_date:
 * @key: a #GMimeCryptoKey
 *
 * Get the expiration date of the key.
 *
 * Returns: the expiration date of the key or %-1 if unknown.
 **/
time_t
g_mime_crypto_key_get_expiration_date (const GMimeCryptoKey *key)
{
	g_return_val_if_fail (key != NULL, (time_t) -1);
	
	return key->expires;
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
	signer->key = g_mime_crypto_key_new (NULL);
	signer->hash_algo = GMIME_CRYPTO_HASH_DEFAULT;
	signer->status = status;
	signer->errors = GMIME_SIGNER_ERROR_NONE;
	signer->trust = GMIME_SIGNER_TRUST_NONE;
	signer->created = (time_t) -1;
	signer->expires = (time_t) -1;
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
	if (signer != NULL) {
		g_mime_crypto_key_free (signer->key);
		g_slice_free (GMimeSigner, signer);
	}
}


/**
 * g_mime_signer_next:
 * @signer: a #GMimeSigner
 *
 * Advance to the next signer.
 *
 * Returns: the next #GMimeSigner or %NULL when complete.
 **/
GMimeSigner *
g_mime_signer_next (GMimeSigner *signer)
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
 * g_mime_signer_get_key:
 * @signer: a #GMimeSigner
 *
 * Get the #GMimeCryptoKey used by the signer.
 *
 * Returns: the key used by the signer.
 **/
const GMimeCryptoKey *
g_mime_signer_get_key (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, NULL);
	
	return signer->key;
}


/**
 * g_mime_signer_set_creation_date:
 * @signer: a #GMimeSigner
 * @created: creation date
 *
 * Set the creation date of the signer's signature.
 **/
void
g_mime_signer_set_creation_date (GMimeSigner *signer, time_t created)
{
	g_return_if_fail (signer != NULL);
	
	signer->created = created;
}


/**
 * g_mime_signer_get_creation_date:
 * @signer: a #GMimeSigner
 *
 * Get the creation date of the signer's signature.
 *
 * Returns: the creation date of the signer's signature or %-1 if
 * unknown.
 **/
time_t
g_mime_signer_get_creation_date (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->created;
}


/**
 * g_mime_signer_set_expiration_date:
 * @signer: a #GMimeSigner
 * @expires: expiration date
 *
 * Set the expiration date of the signer's signature.
 **/
void
g_mime_signer_set_expiration_date (GMimeSigner *signer, time_t expires)
{
	g_return_if_fail (signer != NULL);
	
	signer->expires = expires;
}


/**
 * g_mime_signer_get_expiration_date:
 * @signer: a #GMimeSigner
 *
 * Get the expiration date of the signer's signature.
 *
 * Returns: the expiration date of the signer's signature or %-1 if
 * unknown.
 **/
time_t
g_mime_signer_get_expiration_date (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->expires;
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
 * @validity: signature validity
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
 * @validity: signature validity
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
 * @validity: signature validity
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
 * @validity: signature validity
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
 * @validity: signature validity
 * @signer: signer
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
