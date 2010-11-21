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

#include "gmime-cipher-context.h"
#include "gmime-error.h"


/**
 * SECTION: gmime-cipher-context
 * @title: GMimeCipherContext
 * @short_description: Encryption/signing contexts
 * @see_also:
 *
 * A #GMimeCipherContext is used for encrypting, decrypting, signing
 * and verifying cryptographic signatures.
 **/


static void g_mime_cipher_context_class_init (GMimeCipherContextClass *klass);
static void g_mime_cipher_context_init (GMimeCipherContext *ctx, GMimeCipherContextClass *klass);
static void g_mime_cipher_context_finalize (GObject *object);

static GMimeCipherHash cipher_hash_id (GMimeCipherContext *ctx, const char *hash);

static const char *cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

static int cipher_sign (GMimeCipherContext *ctx, const char *userid,
			GMimeCipherHash hash, GMimeStream *istream,
			GMimeStream *ostream, GError **err);
	
static GMimeSignatureValidity *cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					      GMimeStream *istream, GMimeStream *sigstream,
					      GError **err);
	
static int cipher_encrypt (GMimeCipherContext *ctx, gboolean sign,
			   const char *userid, GPtrArray *recipients,
			   GMimeStream *istream, GMimeStream *ostream,
			   GError **err);

static GMimeSignatureValidity *cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
					       GMimeStream *ostream, GError **err);

static int cipher_import_keys (GMimeCipherContext *ctx, GMimeStream *istream,
			       GError **err);

static int cipher_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
			       GMimeStream *ostream, GError **err);


static GObjectClass *parent_class = NULL;


GType
g_mime_cipher_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeCipherContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_cipher_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeCipherContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_cipher_context_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeCipherContext", &info, 0);
	}
	
	return type;
}


static void
g_mime_cipher_context_class_init (GMimeCipherContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_cipher_context_finalize;
	
	klass->hash_id = cipher_hash_id;
	klass->hash_name = cipher_hash_name;
	klass->sign = cipher_sign;
	klass->verify = cipher_verify;
	klass->encrypt = cipher_encrypt;
	klass->decrypt = cipher_decrypt;
	klass->import_keys = cipher_import_keys;
	klass->export_keys = cipher_export_keys;
}

static void
g_mime_cipher_context_init (GMimeCipherContext *ctx, GMimeCipherContextClass *klass)
{
	ctx->session = NULL;
}

static void
g_mime_cipher_context_finalize (GObject *object)
{
	GMimeCipherContext *ctx = (GMimeCipherContext *) object;
	
	if (ctx->session)
		g_object_unref (ctx->session);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeCipherHash
cipher_hash_id (GMimeCipherContext *ctx, const char *hash)
{
	return GMIME_CIPHER_HASH_DEFAULT;
}


/**
 * g_mime_cipher_context_hash_id:
 * @ctx: a #GMimeCipherContext
 * @hash: hash name
 *
 * Gets the hash id based on the hash name @hash.
 *
 * Returns: the equivalent hash id or #GMIME_CIPHER_HASH_DEFAULT on fail.
 **/
GMimeCipherHash
g_mime_cipher_context_hash_id (GMimeCipherContext *ctx, const char *hash)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), GMIME_CIPHER_HASH_DEFAULT);
	g_return_val_if_fail (hash != NULL, GMIME_CIPHER_HASH_DEFAULT);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->hash_id (ctx, hash);
}


static const char *
cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash)
{
	return NULL;
}


/**
 * g_mime_cipher_context_hash_name:
 * @ctx: a #GMimeCipherContext
 * @hash: hash id
 *
 * Gets the hash name based on the hash id @hash.
 *
 * Returns: the equivalent hash name or %NULL on fail.
 **/
const char *
g_mime_cipher_context_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->hash_name (ctx, hash);
}


static int
cipher_sign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
	     GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Signing is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_context_sign:
 * @ctx: a #GMimeCipherContext
 * @userid: private key to use to sign the stream
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @err: a #GError
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Returns: the #GMimeCipherHash used on success (useful if @hash is
 * specified as #GMIME_CIPHER_HASH_DEFAULT) or %-1 on fail.
 **/
int
g_mime_cipher_context_sign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
			    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->sign (ctx, userid, hash, istream, ostream, err);
}


static GMimeSignatureValidity *
cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash, GMimeStream *istream,
	       GMimeStream *sigstream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Verifying is not supported by this cipher");
	
	return NULL;
}


/**
 * g_mime_cipher_context_verify:
 * @ctx: a #GMimeCipherContext
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
g_mime_cipher_context_verify (GMimeCipherContext *ctx, GMimeCipherHash hash, GMimeStream *istream,
			      GMimeStream *sigstream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->verify (ctx, hash, istream, sigstream, err);
}


static int
cipher_encrypt (GMimeCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
		GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Encryption is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_context_encrypt:
 * @ctx: a #GMimeCipherContext
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
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
g_mime_cipher_context_encrypt (GMimeCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
			       GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->encrypt (ctx, sign, userid, recipients, istream, ostream, err);
}


static GMimeSignatureValidity *
cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
		GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Decryption is not supported by this cipher");
	
	return NULL;
}


/**
 * g_mime_cipher_context_decrypt:
 * @ctx: a #GMimeCipherContext
 * @istream: input/ciphertext stream
 * @ostream: output/cleartext stream
 * @err: a #GError
 *
 * Decrypts the ciphertext input stream and writes the resulting
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
g_mime_cipher_context_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
			       GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), NULL);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->decrypt (ctx, istream, ostream, err);
}


static int
cipher_import_keys (GMimeCipherContext *ctx, GMimeStream *istream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "You may not import keys with this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_context_import_keys:
 * @ctx: a #GMimeCipherContext
 * @istream: input stream (containing keys)
 * @err: a #GError
 *
 * Imports a stream of keys/certificates contained within @istream
 * into the key/certificate database controlled by @ctx.
 *
 * Returns: %0 on success or %-1 on fail.
 **/
int
g_mime_cipher_context_import_keys (GMimeCipherContext *ctx, GMimeStream *istream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->import_keys (ctx, istream, err);
}


static int
cipher_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
		    GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "You may not export keys with this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_context_export_keys:
 * @ctx: a #GMimeCipherContext
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
g_mime_cipher_context_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
				   GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	g_return_val_if_fail (keys != NULL, -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->export_keys (ctx, keys, ostream, err);
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
	signer->created = (time_t) 0;
	signer->expires = (time_t) 0;
	signer->fingerprint = NULL;
	signer->keyid = NULL;
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
	g_free (signer->fingerprint);
	g_free (signer->keyid);
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
 * g_mime_signer_set_created:
 * @signer: a #GMimeSigner
 * @created: creation date
 *
 * Set the signer's key creation date.
 **/
void
g_mime_signer_set_created (GMimeSigner *signer, time_t created)
{
	g_return_if_fail (signer != NULL);
	
	signer->created = created;
}


/**
 * g_mime_signer_get_created:
 * @signer: a #GMimeSigner
 *
 * Get the creation date of the signer's key.
 *
 * Returns: the creation date of the signer's key.
 **/
time_t
g_mime_signer_get_created (const GMimeSigner *signer)
{
	g_return_val_if_fail (signer != NULL, (time_t) -1);
	
	return signer->created;
}


/**
 * g_mime_signer_set_expires:
 * @signer: a #GMimeSigner
 * @expires: expiration date
 *
 * Set the signer's key expiration date.
 **/
void
g_mime_signer_set_expires (GMimeSigner *signer, time_t expires)
{
	g_return_if_fail (signer != NULL);
	
	signer->expires = expires;
}


/**
 * g_mime_signer_get_expires:
 * @signer: a #GMimeSigner
 *
 * Get the expiration date of the signer's key.
 *
 * Returns: the expiration date of the signer's key.
 **/
time_t
g_mime_signer_get_expires (const GMimeSigner *signer)
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
 * as trust and cipher keys.
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
g_mime_signature_validity_add_signer  (GMimeSignatureValidity *validity, GMimeSigner *signer)
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
