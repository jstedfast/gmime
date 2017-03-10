/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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

#include <string.h>

#include "gmime-gpgme-utils.h"
#include "gmime-stream-pipe.h"
#include "gmime-error.h"

#ifdef ENABLE_CRYPTO

#define _(x) x

gpgme_error_t
g_mime_gpgme_passphrase_callback (void *hook, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd)
{
	GMimeCryptoContext *context = (GMimeCryptoContext *) hook;
	GMimeStream *stream;
	gpgme_error_t error;
	GError *err = NULL;
	gboolean rv;
	
	if (context->request_passwd) {
		stream = g_mime_stream_pipe_new (fd);
		rv = context->request_passwd (context, uid_hint, passphrase_info, prev_was_bad, stream, &err);
		g_object_unref (stream);
	} else {
		return GPG_ERR_GENERAL;
	}
	
	if (!rv) {
		error = GPG_ERR_CANCELED;
		g_error_free (err);
	} else {
		error = GPG_ERR_NO_ERROR;
	}
	
	return error;
}


#define KEY_IS_OK(k)   (!((k)->expired || (k)->revoked ||	\
                          (k)->disabled || (k)->invalid))

gpgme_key_t
g_mime_gpgme_get_key_by_name (gpgme_ctx_t ctx, const char *name, gboolean secret, GError **err)
{
	time_t now = time (NULL);
	gpgme_key_t key = NULL;
	gpgme_subkey_t subkey;
	gboolean bad = FALSE;
	gpgme_error_t error;
	int errval = 0;
	
	if ((error = gpgme_op_keylist_start (ctx, name, secret)) != GPG_ERR_NO_ERROR) {
		if (secret)
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list secret keys for \"%s\": %s"), name, gpgme_strerror (error));
		else
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list keys for \"%s\": %s"), name, gpgme_strerror (error));
		return NULL;
	}
	
	while ((error = gpgme_op_keylist_next (ctx, &key)) == GPG_ERR_NO_ERROR) {
		/* check if this key and the relevant subkey are usable */
		if (KEY_IS_OK (key)) {
			subkey = key->subkeys;
			
			while (subkey && ((secret && !subkey->can_sign) ||
					  (!secret && !subkey->can_encrypt)))
				subkey = subkey->next;
			
			if (subkey && KEY_IS_OK (subkey) && 
			    (subkey->expires == 0 || subkey->expires > now))
				break;
			
			if (subkey->expired)
				errval = GPG_ERR_KEY_EXPIRED;
			else
				errval = GPG_ERR_BAD_KEY;
		} else {
			if (key->expired)
				errval = GPG_ERR_KEY_EXPIRED;
			else
				errval = GPG_ERR_BAD_KEY;
		}
		
		gpgme_key_unref (key);
		bad = TRUE;
		key = NULL;
	}
	
	gpgme_op_keylist_end (ctx);
	
	if (error != GPG_ERR_NO_ERROR && error != GPG_ERR_EOF) {
		if (secret)
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list secret keys for \"%s\": %s"), name, gpgme_strerror (error));
		else
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list keys for \"%s\": %s"), name, gpgme_strerror (error));
		
		return NULL;
	}
	
	if (!key) {
		if (strchr (name, '@')) {
			if (bad)
				g_set_error (err, GMIME_GPGME_ERROR, errval,
					     _("A key for %s is present, but it is expired, disabled, revoked or invalid"),
					     name);
			else
				g_set_error (err, GMIME_GPGME_ERROR, GPG_ERR_NOT_FOUND,
					     _("Could not find a key for %s"), name);
		} else {
			if (bad)
				g_set_error (err, GMIME_GPGME_ERROR, errval,
					     _("A key with id %s is present, but it is expired, disabled, revoked or invalid"),
					     name);
			else
				g_set_error (err, GMIME_GPGME_ERROR, GPG_ERR_NOT_FOUND,
					     _("Could not find a key with id %s"), name);
		}
		
		return NULL;
	}
	
	return key;
}

gboolean
g_mime_gpgme_add_signer (gpgme_ctx_t ctx, const char *signer, GError **err)
{
	gpgme_key_t key = NULL;
	gpgme_error_t error;
	
	if (!(key = g_mime_gpgme_get_key_by_name (ctx, signer, TRUE, err)))
		return FALSE;
	
	error = gpgme_signers_add (ctx, key);
	gpgme_key_unref (key);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Failed to add signer \"%s\": %s"), signer, gpgme_strerror (error));
		return FALSE;
	}
	
	return TRUE;
}

static GMimeCertificateTrust
get_trust (gpgme_validity_t trust)
{
	switch (trust) {
	case GPGME_VALIDITY_UNKNOWN:
	default:
		return GMIME_CERTIFICATE_TRUST_NONE;
	case GPGME_VALIDITY_UNDEFINED:
		return GMIME_CERTIFICATE_TRUST_UNDEFINED;
	case GPGME_VALIDITY_NEVER:
		return GMIME_CERTIFICATE_TRUST_NEVER;
	case GPGME_VALIDITY_MARGINAL:
		return GMIME_CERTIFICATE_TRUST_MARGINAL;
	case GPGME_VALIDITY_FULL:
		return GMIME_CERTIFICATE_TRUST_FULLY;
	case GPGME_VALIDITY_ULTIMATE:
		return GMIME_CERTIFICATE_TRUST_ULTIMATE;
	}
}

GMimeSignatureList *
g_mime_gpgme_get_signatures (gpgme_ctx_t ctx, gboolean verify)
{
	GMimeSignatureList *signatures;
	GMimeSignature *signature;
	gpgme_verify_result_t result;
	gpgme_signature_t sig;
	gpgme_subkey_t subkey;
	gpgme_user_id_t uid;
	gpgme_key_t key;
	
	/* get the signature verification results from GpgMe */
	if (!(result = gpgme_op_verify_result (ctx)) || !result->signatures)
		return verify ? g_mime_signature_list_new () : NULL;
	
	/* create a new signature list to return */
	signatures = g_mime_signature_list_new ();
	
	sig = result->signatures;
	
	while (sig != NULL) {
		signature = g_mime_signature_new ();
		g_mime_signature_list_add (signatures, signature);
		g_mime_signature_set_status (signature, (GMimeSignatureStatus) sig->summary);
		g_mime_signature_set_expires (signature, sig->exp_timestamp);
		g_mime_signature_set_created (signature, sig->timestamp);
		
		g_mime_certificate_set_pubkey_algo (signature->cert, (GMimePubKeyAlgo) sig->pubkey_algo);
		g_mime_certificate_set_digest_algo (signature->cert, (GMimeDigestAlgo) sig->hash_algo);
		g_mime_certificate_set_fingerprint (signature->cert, sig->fpr);
		
		if (gpgme_get_key (ctx, sig->fpr, &key, 0) == GPG_ERR_NO_ERROR && key) {
			/* get more signer info from their signing key */
			g_mime_certificate_set_trust (signature->cert, get_trust (key->owner_trust));
			g_mime_certificate_set_issuer_serial (signature->cert, key->issuer_serial);
			g_mime_certificate_set_issuer_name (signature->cert, key->issuer_name);
			
			/* get the keyid, name, and email address */
			uid = key->uids;
			while (uid) {
				if (uid->name && *uid->name)
					g_mime_certificate_set_name (signature->cert, uid->name);
				
				if (uid->email && *uid->email)
					g_mime_certificate_set_email (signature->cert, uid->email);
				
				if (uid->uid && *uid->uid)
					g_mime_certificate_set_key_id (signature->cert, uid->uid);
				
				if (signature->cert->name && signature->cert->email && signature->cert->keyid)
					break;
				
				uid = uid->next;
			}
			
			/* get the subkey used for signing */
			subkey = key->subkeys;
			while (subkey && !subkey->can_sign)
				subkey = subkey->next;
			
			if (subkey) {
				g_mime_certificate_set_created (signature->cert, subkey->timestamp);
				g_mime_certificate_set_expires (signature->cert, subkey->expires);
			}
			
			gpgme_key_unref (key);
		} else {
			/* If we don't have the signer's public key, then we can't tell what
			 * the status is, so set it to ERROR if it hasn't already been
			 * designated as BAD. */
			g_mime_certificate_set_trust (signature->cert, GMIME_CERTIFICATE_TRUST_UNDEFINED);
		}
		
		sig = sig->next;
	}
	
	return signatures;
}

void
g_mime_gpgme_keylist_free (gpgme_key_t *keys)
{
	gpgme_key_t *key = keys;
	
	while (*key != NULL) {
		gpgme_key_unref (*key);
		key++;
	}
	
	g_free (keys);
}

GMimeDecryptResult *
g_mime_gpgme_get_decrypt_result (gpgme_ctx_t ctx)
{
	GMimeDecryptResult *result;
	gpgme_decrypt_result_t res;
	gpgme_recipient_t recipient;
	GMimeCertificate *cert;
	
	result = g_mime_decrypt_result_new ();
	result->recipients = g_mime_certificate_list_new ();
	result->signatures = g_mime_gpgme_get_signatures (ctx, FALSE);
	
	// TODO: ciper, mdc
	
	if (!(res = gpgme_op_decrypt_result (ctx)) || !res->recipients)
		return result;
	
	if (res->session_key)
		result->session_key = g_strdup (res->session_key);
	
	recipient = res->recipients;
	while (recipient != NULL) {
		cert = g_mime_certificate_new ();
		g_mime_certificate_list_add (result->recipients, cert);
		
		g_mime_certificate_set_pubkey_algo (cert, (GMimePubKeyAlgo) recipient->pubkey_algo);
		g_mime_certificate_set_key_id (cert, recipient->keyid);
		
		recipient = recipient->next;
	}
	
	return result;
}

#endif /* ENABLE_CRYPTO */
