/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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

static void cipher_destroy (GMimeObject *object);

static GMimeObject object_template = {
	0, 0, cipher_destroy
};


/**
 * g_mime_cipher_context_construct:
 * @context: GMimeCipherContext
 * @context_template: template
 * @type: context type
 *
 * Constucts the GMimeCipherContext
 **/
void
g_mime_cipher_context_construct (GMimeCipherContext *context, GMimeCipherContext *context_template, int type)
{
	context->destroy = context_template->destroy;
	context->get_passwd = context_template->get_passwd;
	context->sign = context_template->sign;
	context->clearsign = context_template->clearsign;
	context->verify = context_template->verify;
	context->encrypt = context_template->encrypt;
	context->decrypt = context_template->decrypt;
	
	g_mime_object_construct (GMIME_OBJECT (context), &object_template, type);
}


static void
cipher_destroy (GMimeObject *object)
{
	GMimeCipherContext *ctx = (GMimeCipherContext *) object;
	
	if (ctx->destroy)
		ctx->destroy (ctx);
	else
		g_free (object);
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
 * Return value: 0 for success or -1 for failure.
 **/
int
g_mime_cipher_sign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
		    GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (ctx != NULL, -1);
	
	if (ctx->sign) {
		return ctx->sign (ctx, userid, hash, istream, ostream, ex);
	} else {
		g_warning ("Cipher context does not have a sign() method");
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Signing is not supported by this cipher");
		return -1;
	}
}


/**
 * g_mime_cipher_clearsign:
 * @ctx: Cipher Context
 * @userid: key id or email address of the private key to sign with
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * Clearsigns the input stream and writes the resulting clearsign to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
g_mime_cipher_clearsign (GMimeCipherContext *ctx, const char *userid, GMimeCipherHash hash,
			 GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (ctx != NULL, -1);

	if (ctx->clearsign) {
		return ctx->clearsign (ctx, userid, hash, istream, ostream, ex);
	} else {
		g_warning ("Cipher context does not have a clearsign() method");
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Clearsigning is not supported by this cipher");
		return -1;
	}
}


/**
 * g_mime_cipher_verify:
 * @ctx: Cipher Context
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @ex: exception
 *
 * Verifies the signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Return value: a GMimeCipherValidity structure containing information
 * about the integrity of the input stream or %NULL on failure to
 * execute at all.
 **/
GMimeCipherValidity *
g_mime_cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash, GMimeStream *istream,
		      GMimeStream *sigstream, GMimeException *ex)
{
	g_return_val_if_fail (ctx != NULL, NULL);
	
	if (ctx->verify) {
		return ctx->verify (ctx, hash, istream, sigstream, ex);
	} else {
		g_warning ("Cipher context does not have a verify() method");
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Verifying is not supported by this cipher");
		return NULL;
	}
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
 * Return value: 0 for success or -1 for failure.
 **/
int
g_mime_cipher_encrypt (GMimeCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
		       GMimeStream *istream, GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (ctx != NULL, -1);
	
	if (ctx->encrypt) {
		return ctx->encrypt (ctx, sign, userid, recipients, istream, ostream, ex);
	} else {
		g_warning ("Cipher context does not have an encrypt() method");
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Encryption is not supported by this cipher");
		return -1;
	}
}


/**
 * g_mime_cipher_decrypt:
 * @ctx: Cipher Context
 * @ciphertext: ciphertext stream (ie input stream)
 * @cleartext: cleartext stream (ie output stream)
 * @ex: exception
 *
 * Decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
g_mime_cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
		       GMimeStream *ostream, GMimeException *ex)
{
	g_return_val_if_fail (ctx != NULL, -1);
	
	if (ctx->decrypt) {
		return ctx->decrypt (ctx, istream, ostream, ex);
	} else {
		g_warning ("Cipher context does not have a decrypt() method");
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Decryption is not supported by this cipher");
		return -1;
	}
}


/* Cipher Validity stuff */
struct _GMimeCipherValidity {
	gboolean valid;
	char *description;
};

GMimeCipherValidity *
g_mime_cipher_validity_new (void)
{
	GMimeCipherValidity *validity;
	
	validity = g_new (GMimeCipherValidity, 1);
	validity->valid = FALSE;
	validity->description = NULL;
	
	return validity;
}

void
g_mime_cipher_validity_init (GMimeCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	validity->description = NULL;
}

gboolean
g_mime_cipher_validity_get_valid (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return FALSE;
	
	return validity->valid;
}

void
g_mime_cipher_validity_set_valid (GMimeCipherValidity *validity, gboolean valid)
{
	g_assert (validity != NULL);
	
	validity->valid = valid;
}

char *
g_mime_cipher_validity_get_description (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return NULL;
	
	return validity->description;
}

void
g_mime_cipher_validity_set_description (GMimeCipherValidity *validity, const char *description)
{
	g_assert (validity != NULL);
	
	g_free (validity->description);
	validity->description = g_strdup (description);
}

void
g_mime_cipher_validity_clear (GMimeCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	g_free (validity->description);
	validity->description = NULL;
}

void
g_mime_cipher_validity_free (GMimeCipherValidity *validity)
{
	if (validity == NULL)
		return;
	
	g_free (validity->description);
	g_free (validity);
}
