/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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

static int cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
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
 * @ctx: Cipher Context
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
 * @ctx: Cipher Context
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
 * @ctx: Cipher Context
 * @userid: private key to use to sign the stream
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @err: exception
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Returns: %0 on success or %-1 on fail.
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
 * @ctx: Cipher Context
 * @hash: secure hash used
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @err: exception
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
 * @ctx: Cipher Context
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @recipients: an array of recipient key ids and/or email addresses
 * @istream: cleartext input stream
 * @ostream: ciphertext output stream
 * @err: exception
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


static int
cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
		GMimeStream *ostream, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Decryption is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_context_decrypt:
 * @ctx: Cipher Context
 * @istream: input/ciphertext stream
 * @ostream: output/cleartext stream
 * @err: exception
 *
 * Decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * Returns: %0 on success or %-1 for fail.
 **/
int
g_mime_cipher_context_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
			       GMimeStream *ostream, GError **err)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
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
 * @ctx: Cipher Context
 * @istream: input stream (containing keys)
 * @err: exception
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
 * @ctx: Cipher Context
 * @keys: an array of key ids
 * @ostream: output stream
 * @err: exception
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
 * Allocates an new GMimeSigner.
 *
 * Returns: a new #GMimeSigner.
 **/
GMimeSigner *
g_mime_signer_new (void)
{
	GMimeSigner *signer;
	
	signer = g_new (GMimeSigner, 1);
	signer->status = GMIME_SIGNER_STATUS_NONE;
	signer->errors = GMIME_SIGNER_ERROR_NONE;
	signer->trust = GMIME_SIGNER_TRUST_NONE;
	signer->sig_created = (time_t) 0;
	signer->sig_expire = (time_t) 0;
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
	g_free (signer);
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
	
	validity = g_new (GMimeSignatureValidity, 1);
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
		g_free (signer->fingerprint);
		g_free (signer->keyid);
		g_free (signer->name);
		g_free (signer);
		signer = next;
	}
	
	g_free (validity->details);
	
	g_free (validity);
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
g_mime_signature_validity_get_status (GMimeSignatureValidity *validity)
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
g_mime_signature_validity_get_details (GMimeSignatureValidity *validity)
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
g_mime_signature_validity_get_signers (GMimeSignatureValidity *validity)
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
