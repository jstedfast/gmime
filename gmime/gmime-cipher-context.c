/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-cipher-context.h"

static void g_mime_cipher_context_class_init (GMimeCipherContextClass *klass);
static void g_mime_cipher_context_init (GMimeCipherContext *ctx, GMimeCipherContextClass *klass);
static void g_mime_cipher_context_finalize (GObject *object);

static GMimeCipherHash cipher_hash_id (GMimeCipherContext *ctx, const char *hash);

static const char *cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

static int cipher_sign (GMimeCipherContext *ctx, const char *userid,
			GMimeCipherHash hash, GMimeStream *istream,
			GMimeStream *ostream, GMimeException *ex);
	
static GMimeCipherValidity *cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					   GMimeStream *istream, GMimeStream *sigstream,
					   GMimeException *ex);
	
static int cipher_encrypt (GMimeCipherContext *ctx, gboolean sign,
			   const char *userid, GPtrArray *recipients,
			   GMimeStream *istream, GMimeStream *ostream,
			   GMimeException *ex);

static int cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
			   GMimeStream *ostream, GMimeException *ex);

static int cipher_import (GMimeCipherContext *ctx, GMimeStream *istream,
			  GMimeException *ex);

static int cipher_export (GMimeCipherContext *ctx, GPtrArray *keys,
			  GMimeStream *ostream, GMimeException *ex);


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
	klass->import = cipher_import;
	klass->export = cipher_export;
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
 * g_mime_cipher_hash_id:
 * @ctx: Cipher Context
 * @hash: hash name
 *
 * Gets the hash id based on the hash name @hash.
 *
 * Returns the equivalent hash id or #GMIME_CIPHER_HASH_DEFAULT on fail.
 **/
GMimeCipherHash
g_mime_cipher_hash_id (GMimeCipherContext *ctx, const char *hash)
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
 * g_mime_cipher_hash_name:
 * @ctx: Cipher Context
 * @hash: hash id
 *
 * Gets the hash name based on the hash id @hash.
 *
 * Returns the equivalent hash name or %NULL on fail.
 **/
const char *
g_mime_cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->hash_name (ctx, hash);
}


static int
cipher_sign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
	     GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "Signing is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_sign:
 * @ctx: Cipher Context
 * @userid: private key to use to sign the stream
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_cipher_sign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
		    GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->sign (ctx, userid, hash, istream, ostream, ex);
}


static GMimeCipherValidity *
cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash, GMimeStream *istream,
	       GMimeStream *sigstream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "Verifying is not supported by this cipher");
	
	return NULL;
}


/**
 * g_mime_cipher_verify:
 * @ctx: Cipher Context
 * @hash: secure hash used
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @ex: exception
 *
 * Verifies the signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Returns a GMimeCipherValidity structure containing information
 * about the integrity of the input stream or %NULL on failure to
 * execute at all.
 **/
GMimeCipherValidity *
g_mime_cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash, GMimeStream *istream,
		      GMimeStream *sigstream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (sigstream), NULL);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->verify (ctx, hash, istream, sigstream, ex);
}


static int
cipher_encrypt (GMimeCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
		GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "Encryption is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_encrypt:
 * @ctx: Cipher Context
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @recipients: an array of recipient key ids and/or email addresses
 * @istream: cleartext input stream
 * @ostream: ciphertext output stream
 * @ex: exception
 *
 * Encrypts (and optionally signs) the cleartext input stream and
 * writes the resulting ciphertext to the output stream.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_cipher_encrypt (GMimeCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
		       GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->encrypt (ctx, sign, userid, recipients, istream, ostream, ex);
}


static int
cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
		GMimeStream *ostream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "Decryption is not supported by this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_decrypt:
 * @ctx: Cipher Context
 * @istream: input/ciphertext stream
 * @ostream: output/cleartext stream
 * @ex: exception
 *
 * Decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * Returns 0 on success or -1 for fail.
 **/
int
g_mime_cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
		       GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->decrypt (ctx, istream, ostream, ex);
}


static int
cipher_import (GMimeCipherContext *ctx, GMimeStream *istream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "You may not import keys with this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_import:
 * @ctx: Cipher Context
 * @istream: input stream (containing keys)
 * @ex: exception
 *
 * Imports a stream of keys/certificates contained within @istream
 * into the key/certificate database controlled by @ctx.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_cipher_import (GMimeCipherContext *ctx, GMimeStream *istream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (istream), -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->import (ctx, istream, ex);
}


static int
cipher_export (GMimeCipherContext *ctx, GPtrArray *keys,
	       GMimeStream *ostream, GMimeException *ex)
{
	g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
			      "You may not export keys with this cipher");
	
	return -1;
}


/**
 * g_mime_cipher_export:
 * @ctx: Cipher Context
 * @keys: an array of key ids
 * @ostream: output stream
 * @ex: exception
 *
 * Exports the keys/certificates in @keys to the stream @ostream from
 * the key/certificate database controlled by @ctx.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
g_mime_cipher_export (GMimeCipherContext *ctx, GPtrArray *keys,
		      GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (ostream), -1);
	g_return_val_if_fail (keys != NULL, -1);
	
	return GMIME_CIPHER_CONTEXT_GET_CLASS (ctx)->export (ctx, keys, ostream, ex);
}


/* Cipher Validity stuff */
struct _GMimeCipherValidity {
	gboolean valid;
	char *description;
};


/**
 * g_mime_cipher_validity_new:
 *
 * Creates a new validity structure.
 *
 * Returns a new validity structure.
 **/
GMimeCipherValidity *
g_mime_cipher_validity_new (void)
{
	GMimeCipherValidity *validity;
	
	validity = g_new (GMimeCipherValidity, 1);
	validity->valid = FALSE;
	validity->description = NULL;
	
	return validity;
}


/**
 * g_mime_cipher_validity_init:
 * @validity: validity structure
 *
 * Initializes the validity structure.
 **/ 
void
g_mime_cipher_validity_init (GMimeCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	validity->description = NULL;
}


/**
 * g_mime_cipher_validity_get_valid:
 * @validity: validity structure
 *
 * Gets the validity of the validity structure @validity.
 *
 * Returns %TRUE if @validity is valid or %FALSE otherwise.
 **/
gboolean
g_mime_cipher_validity_get_valid (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return FALSE;
	
	return validity->valid;
}


/**
 * g_mime_cipher_validity_set_valid:
 * @validity: validity structure
 * @valid: %TRUE if valid else %FALSE
 *
 * Sets the validness on the validity structure.
 **/
void
g_mime_cipher_validity_set_valid (GMimeCipherValidity *validity, gboolean valid)
{
	g_assert (validity != NULL);
	
	validity->valid = valid;
}


/**
 * g_mime_cipher_validity_get_description:
 * @validity: validity structure
 *
 * Gets the description set on the validity structure @validity.
 *
 * Returns any description set on the validity structure.
 **/
const char *
g_mime_cipher_validity_get_description (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return NULL;
	
	return validity->description;
}


/**
 * g_mime_cipher_validity_set_description:
 * @validity: validity structure
 * @description: validity description
 *
 * Sets the description on the validity structure.
 **/
void
g_mime_cipher_validity_set_description (GMimeCipherValidity *validity, const char *description)
{
	g_assert (validity != NULL);
	
	g_free (validity->description);
	validity->description = g_strdup (description);
}


/**
 * g_mime_cipher_validity_clear:
 * @validity: validity structure
 *
 * Clears the contents of the validity structure.
 **/
void
g_mime_cipher_validity_clear (GMimeCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	g_free (validity->description);
	validity->description = NULL;
}


/**
 * g_mime_cipher_validity_free:
 * @validity: validity structure
 *
 * Frees the memory used by @validity back to the system.
 **/
void
g_mime_cipher_validity_free (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return;
	
	g_free (validity->description);
	g_free (validity);
}
