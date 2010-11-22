/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
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

static int crypto_sign (GMimeCryptoContext *ctx, const char *userid,
			GMimeCryptoHash hash, GMimeStream *istream,
			GMimeStream *ostream, GError **err);
	
static GMimeSignatureValidity *crypto_verify (GMimeCryptoContext *ctx, GMimeCryptoHash hash,
					      GMimeStream *istream, GMimeStream *sigstream,
					      GError **err);
	
static int crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign,
			   const char *userid, GPtrArray *recipients,
			   GMimeStream *istream, GMimeStream *ostream,
			   GError **err);

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
 * @hash: preferred Message-Integrity-Check hash algorithm
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
 * @hash: secure hash used
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
crypto_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
		GMimeStream *istream, GMimeStream *ostream, GError **err)
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
 * @recipients: an array of recipient key ids and/or email addresses
 * @istream: cleartext input stream
 * @ostream: cryptotext output stream
 * @err: a #GError
 *
 * Encrypts (and optionally signs) the cleartext input stream and
 * writes the resulting cryptotext to the output stream.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_crypto_context_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
			       GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CRYPTO_CONTEXT_GET_CLASS (ctx)->encrypt (ctx, sign, userid, recipients, istream, ostream, err);
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
 * @istream: input/cryptotext stream
 * @ostream: output/cleartext stream
 * @err: a #GError
 *
 * Decrypts the cryptotext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * If the encrypted input stream was also signed, the returned
 * #GMimeSignatureValidity will have signer information included and
 * the signature status will be one of #GMIME_SIGNATURE_STATUS_GOOD,
 * #GMIME_SIGNATURE_STATUS_BAD, or #GMIME_SIGNATURE_STATUS_UNKNOWN.
 *
 * If the encrypted input text was not signed, then the signature
 * status of the returned #GMimeSignatureValidity will be
 * #GMIME_SIGNATURE_STATUS_NONE.
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
 * g_mime_signer_new:
 *
 * Allocates an new #GMimeSigner.
 *
 * Returns: a new #GMimeSigner.
 **/
GMimeSigner *
g_mime_signer_new (void)
{
	GMimeSigner *signer;
	
	signer = g_slice_new (GMimeSigner);
	signer->status = GMIME_SIGNER_STATUS_NONE;
	signer->errors = GMIME_SIGNER_ERROR_NONE;
	signer->trust = GMIME_SIGNER_TRUST_NONE;
	signer->sig_created = (time_t) 0;
	signer->sig_expires = (time_t) 0;
	signer->key_created = (time_t) 0;
	signer->key_expires = (time_t) 0;
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
 * @signer: signer
 *
 * Free's the singleton signer.
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
	g_return_val_if_fail (signer != NULL, GMIME_SIGNER_STATUS_NONE);
	
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
 * Get the signer errors.
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
 * Returns: the creation date of the signer's signature.
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
 * Returns: the expiration date of the signer's signature.
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
 * Returns: the creation date of the signer's key.
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
 * Returns: the expiration date of the signer's key.
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
	validity->status = GMIME_SIGNATURE_STATUS_NONE;
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
 * g_mime_signature_validity_get_status:
 * @validity: signature validity
 *
 * Gets the signature status (GOOD, BAD, UNKNOWN).
 *
 * Returns: a #GMimeSignatureStatus value.
 **/
GMimeSignatureStatus
g_mime_signature_validity_get_status (const GMimeSignatureValidity *validity)
{
	g_return_val_if_fail (validity != NULL, GMIME_SIGNATURE_STATUS_NONE);
	
	return validity->status;
}


/**
 * g_mime_signature_validity_set_status:
 * @validity: signature validity
 * @status: GOOD, BAD or UNKNOWN
 *
 * Sets the status of the signature on @validity.
 **/
void
g_mime_signature_validity_set_status (GMimeSignatureValidity *validity, GMimeSignatureStatus status)
{
	g_return_if_fail (status != GMIME_SIGNATURE_STATUS_NONE);
	g_return_if_fail (validity != NULL);
	
	validity->status = status;
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
 * as trust and crypto keys.
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
 * Adds @signer to the list of signers on @validity.
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
