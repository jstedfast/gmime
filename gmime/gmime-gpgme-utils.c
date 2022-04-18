/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

static ssize_t
g_mime_gpgme_stream_read (void *stream, void *buffer, size_t size)
{
	return g_mime_stream_read ((GMimeStream *) stream, (char *) buffer, size);
}

static ssize_t
g_mime_gpgme_stream_write (void *stream, const void *buffer, size_t size)
{
	return g_mime_stream_write ((GMimeStream *) stream, (const char *) buffer, size);
}

static off_t
g_mime_gpgme_stream_seek (void *stream, off_t offset, int whence)
{
	switch (whence) {
	case SEEK_SET:
		return (off_t) g_mime_stream_seek ((GMimeStream *) stream, (gint64) offset, GMIME_STREAM_SEEK_SET);
	case SEEK_CUR:
		return (off_t) g_mime_stream_seek ((GMimeStream *) stream, (gint64) offset, GMIME_STREAM_SEEK_CUR);
	case SEEK_END:
		return (off_t) g_mime_stream_seek ((GMimeStream *) stream, (gint64) offset, GMIME_STREAM_SEEK_END);
	default:
		return -1;
	}
}

static void
g_mime_gpgme_stream_free (void *stream)
{
	/* no-op */
}

static struct gpgme_data_cbs gpg_stream_funcs = {
	g_mime_gpgme_stream_read,
	g_mime_gpgme_stream_write,
	g_mime_gpgme_stream_seek,
	g_mime_gpgme_stream_free
};

gpgme_error_t
g_mime_gpgme_passphrase_callback (void *hook, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd)
{
	GMimeCryptoContext *context = (GMimeCryptoContext *) hook;
	GMimeStream *stream;
	gpgme_error_t error;
	GError *err = NULL;
	gboolean rv;
	
	stream = g_mime_stream_pipe_new (fd);
	g_mime_stream_pipe_set_owner ((GMimeStreamPipe *) stream, FALSE);
	rv = context->request_passwd (context, uid_hint, passphrase_info, prev_was_bad, stream, &err);
	g_object_unref (stream);
	
	if (!rv) {
		error = GPG_ERR_CANCELED;
		g_error_free (err);
	} else {
		error = GPG_ERR_NO_ERROR;
	}
	
	return error;
}


static gboolean
g_mime_gpgme_key_is_usable (gpgme_key_t key, gboolean secret, time_t now, gpgme_error_t *err)
{
	gpgme_subkey_t subkey;
	
	*err = GPG_ERR_NO_ERROR;
	
	/* first, check the state of the key itself... */
	if (key->expired)
		*err = GPG_ERR_KEY_EXPIRED;
	else if (key->revoked)
		*err = GPG_ERR_CERT_REVOKED;
	else if (key->disabled)
		*err = GPG_ERR_KEY_DISABLED;
	else if (key->invalid)
		*err = GPG_ERR_BAD_KEY;
	
	if (*err != GPG_ERR_NO_ERROR)
		return FALSE;
	
	/* now check if there is a subkey that we can use */
	subkey = key->subkeys;
	
	while (subkey) {
		if ((secret && subkey->can_sign) || (!secret && subkey->can_encrypt)) {
			if (subkey->expired || (subkey->expires != 0 && subkey->expires <= now))
				*err = GPG_ERR_KEY_EXPIRED;
			else if (subkey->revoked)
				*err = GPG_ERR_CERT_REVOKED;
			else if (subkey->disabled)
				*err = GPG_ERR_KEY_DISABLED;
			else if (subkey->invalid)
				*err = GPG_ERR_BAD_KEY;
			else
				return TRUE;
		}
		
		subkey = subkey->next;
	}
	
	if (*err == GPG_ERR_NO_ERROR)
		*err = GPG_ERR_BAD_KEY;
	
	return FALSE;
}

/* Note: this function based on code in Balsa written by Albrecht Dreß. */
static gpgme_key_t
g_mime_gpgme_get_key_by_name (gpgme_ctx_t ctx, const char *name, gboolean secret, GError **err)
{
	gpgme_error_t key_error = GPG_ERR_NO_ERROR;
	time_t now = time (NULL);
	gpgme_key_t key = NULL;
	gboolean found = FALSE;
	gpgme_error_t error;
	
	if ((error = gpgme_op_keylist_start (ctx, name, secret)) != GPG_ERR_NO_ERROR) {
		if (secret) {
			g_set_error (err, GMIME_GPGME_ERROR, error,
				     _("Could not list secret keys for \"%s\": %s"),
				     name, gpgme_strerror (error));
		} else {
			g_set_error (err, GMIME_GPGME_ERROR, error,
				     _("Could not list keys for \"%s\": %s"),
				     name, gpgme_strerror (error));
		}
		
		return NULL;
	}
	
	while ((error = gpgme_op_keylist_next (ctx, &key)) == GPG_ERR_NO_ERROR) {
		/* check if this key and the relevant subkey are usable */
		if (g_mime_gpgme_key_is_usable (key, secret, now, &key_error))
			break;
		
		gpgme_key_unref (key);
		found = TRUE;
		key = NULL;
	}
	
	gpgme_op_keylist_end (ctx);
	
	if (error != GPG_ERR_NO_ERROR && error != GPG_ERR_EOF) {
		if (secret) {
			g_set_error (err, GMIME_GPGME_ERROR, error,
				     _("Could not list secret keys for \"%s\": %s"),
				     name, gpgme_strerror (error));
		} else {
			g_set_error (err, GMIME_GPGME_ERROR, error,
				     _("Could not list keys for \"%s\": %s"),
				     name, gpgme_strerror (error));
		}
		
		return NULL;
	}
	
	if (!key) {
		if (strchr (name, '@')) {
			if (found && key_error != GPG_ERR_NO_ERROR) {
				g_set_error (err, GMIME_GPGME_ERROR, key_error,
					     _("A key for %s is present, but it is expired, disabled, revoked or invalid"),
					     name);
			} else {
				g_set_error (err, GMIME_GPGME_ERROR, GPG_ERR_NOT_FOUND,
					     _("Could not find a suitable key for %s"), name);
			}
		} else {
			if (found && key_error != GPG_ERR_NO_ERROR) {
				g_set_error (err, GMIME_GPGME_ERROR, key_error,
					     _("A key with id %s is present, but it is expired, disabled, revoked or invalid"),
					     name);
			} else {
				g_set_error (err, GMIME_GPGME_ERROR, GPG_ERR_NOT_FOUND,
					     _("Could not find a suitable key with id %s"), name);
			}
		}
		
		return NULL;
	}
	
	return key;
}

static gboolean
g_mime_gpgme_add_signer (gpgme_ctx_t ctx, const char *signer, GError **err)
{
	gpgme_key_t key = NULL;
	gpgme_error_t error;
	
	if (!(key = g_mime_gpgme_get_key_by_name (ctx, signer, TRUE, err)))
		return FALSE;
	
	error = gpgme_signers_add (ctx, key);
	gpgme_key_unref (key);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error,_("Failed to add signer \"%s\": %s"),
			     signer, gpgme_strerror (error));
		return FALSE;
	}
	
	return TRUE;
}

int
g_mime_gpgme_sign (gpgme_ctx_t ctx, gpgme_sig_mode_t mode, const char *userid,
		   GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	gpgme_sign_result_t result;
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	if (!g_mime_gpgme_add_signer (ctx, userid, err))
		return -1;
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
			     gpgme_strerror (error));
		gpgme_signers_clear (ctx);
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream: %s"),
			     gpgme_strerror (error));
		gpgme_data_release (input);
		gpgme_signers_clear (ctx);
		return -1;
	}
	
	/* sign the input stream */
	error = gpgme_op_sign (ctx, input, output, mode);
	gpgme_data_release (output);
	gpgme_data_release (input);
	gpgme_signers_clear (ctx);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Signing failed: %s"), gpgme_strerror (error));
		return -1;
	}
	
	/* return the digest algorithm used for signing */
	result = gpgme_op_sign_result (ctx);
	
	return (GMimeDigestAlgo) result->signatures->hash_algo;
}


/* return TRUE iff a < b */
static gboolean
_gpgv_lt(gpgme_validity_t a, gpgme_validity_t b) {
	switch (a) {
	case GPGME_VALIDITY_NEVER:
		return b != GPGME_VALIDITY_NEVER;
	case GPGME_VALIDITY_UNKNOWN:
	case GPGME_VALIDITY_UNDEFINED:
		switch (b) {
		case GPGME_VALIDITY_NEVER:
		case GPGME_VALIDITY_UNKNOWN:
		case GPGME_VALIDITY_UNDEFINED:
			return FALSE;
		default:
			return TRUE;
		}
	case GPGME_VALIDITY_MARGINAL:
		switch (b) {
		case GPGME_VALIDITY_NEVER:
		case GPGME_VALIDITY_UNKNOWN:
		case GPGME_VALIDITY_UNDEFINED:
		case GPGME_VALIDITY_MARGINAL:
			return FALSE;
		default:
			return TRUE;
		}
	case GPGME_VALIDITY_FULL:
		return b == GPGME_VALIDITY_ULTIMATE;
	case GPGME_VALIDITY_ULTIMATE:
		return FALSE;
	default:
		g_assert_not_reached();
		return FALSE;
	}
}


static GMimeSignatureList *
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
		g_object_unref (signature);
		
		g_mime_signature_set_status (signature, (GMimeSignatureStatus) sig->summary);
		g_mime_signature_set_expires (signature, sig->exp_timestamp);
		g_mime_signature_set_created (signature, sig->timestamp);
		
		g_mime_certificate_set_pubkey_algo (signature->cert, (GMimePubKeyAlgo) sig->pubkey_algo);
		g_mime_certificate_set_digest_algo (signature->cert, (GMimeDigestAlgo) sig->hash_algo);
		g_mime_certificate_set_fingerprint (signature->cert, sig->fpr);
		g_mime_certificate_set_key_id (signature->cert, sig->fpr);
		
		if (gpgme_get_key (ctx, sig->fpr, &key, 0) == GPG_ERR_NO_ERROR && key) {
			/* get more signer info from their signing key */
			g_mime_certificate_set_trust (signature->cert, (GMimeTrust) key->owner_trust);
			g_mime_certificate_set_issuer_serial (signature->cert, key->issuer_serial);
			g_mime_certificate_set_issuer_name (signature->cert, key->issuer_name);
			
			gpgme_validity_t validity = GPGME_VALIDITY_NEVER;
			gboolean founduid = FALSE;
 			
			/* get the most valid name, email address, and full user id */
			uid = key->uids;
			while (uid) {
				if (!founduid || !_gpgv_lt (uid->validity, validity)) {
					/* this is as good as the best UID we've found so far */
					founduid = TRUE;
					if (_gpgv_lt (validity, uid->validity)) {
						/* this is actually better than the last best,
						   so clear all the previously-found uids */
						g_mime_certificate_set_name (signature->cert, NULL);
						g_mime_certificate_set_email (signature->cert, NULL);
						g_mime_certificate_set_user_id (signature->cert, NULL);
					}
					validity = uid->validity;

					if (uid->name && *uid->name && !g_mime_certificate_get_name (signature->cert))
						g_mime_certificate_set_name (signature->cert, uid->name);
					
					if (uid->address && *uid->address && !g_mime_certificate_get_email (signature->cert))
						g_mime_certificate_set_email (signature->cert, uid->address);

					if (uid->email && *uid->email && !g_mime_certificate_get_email (signature->cert))
						g_mime_certificate_set_email (signature->cert, uid->email);
					
					if (uid->uid && *uid->uid && !g_mime_certificate_get_user_id (signature->cert))
						g_mime_certificate_set_user_id (signature->cert, uid->uid);
					
				}
				uid = uid->next;
			}
			g_mime_certificate_set_id_validity (signature->cert, (GMimeValidity)(validity));
			
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
			g_mime_certificate_set_trust (signature->cert, GMIME_TRUST_UNDEFINED);
		}
		
		sig = sig->next;
	}
	
	return signatures;
}

GMimeSignatureList *
g_mime_gpgme_verify (gpgme_ctx_t ctx, GMimeVerifyFlags flags, GMimeStream *istream, GMimeStream *sigstream,
		     GMimeStream *ostream, GError **err)
{
	gpgme_data_t sig, signed_text, plain;
	gpgme_error_t error;
	
	if (sigstream != NULL) {
		/* if @sigstream is non-NULL, then it is a detached signature */
		if ((error = gpgme_data_new_from_cbs (&signed_text, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
				     gpgme_strerror (error));
			return NULL;
		}
		
		if ((error = gpgme_data_new_from_cbs (&sig, &gpg_stream_funcs, sigstream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open signature stream: %s"),
				     gpgme_strerror (error));
			gpgme_data_release (signed_text);
			return NULL;
		}
		
		plain = NULL;
	} else if (ostream != NULL) {
		/* if @ostream is non-NULL, then we are expected to write the extracted plaintext to it */
		if ((error = gpgme_data_new_from_cbs (&sig, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
				     gpgme_strerror (error));
			return NULL;
		}
		
		if ((error = gpgme_data_new_from_cbs (&plain, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream: %s"),
				     gpgme_strerror (error));
			gpgme_data_release (sig);
			return NULL;
		}
		
		signed_text = NULL;
	} else {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL, _("Missing signature stream or output stream"));
		return NULL;
	}
	
	gpgme_set_offline (ctx, (flags & GMIME_VERIFY_ENABLE_ONLINE_CERTIFICATE_CHECKS) == 0);
	
	error = gpgme_op_verify (ctx, sig, signed_text, plain);
	if (signed_text)
		gpgme_data_release (signed_text);
	if (plain)
		gpgme_data_release (plain);
	gpgme_data_release (sig);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not verify signature: %s"),
			     gpgme_strerror (error));
		return NULL;
	}
	
	/* get/return the signatures */
	return g_mime_gpgme_get_signatures (ctx, TRUE);
}

static void
g_mime_gpgme_keylist_free (gpgme_key_t *keys)
{
	gpgme_key_t *key = keys;
	
	while (*key != NULL) {
		gpgme_key_unref (*key);
		key++;
	}
	
	g_free (keys);
}

int
g_mime_gpgme_encrypt (gpgme_ctx_t ctx, gboolean sign, const char *userid,
		      GMimeEncryptFlags flags, GPtrArray *recipients,
		      GMimeStream *istream, GMimeStream *ostream,
		      GError **err)
{
	gpgme_encrypt_flags_t encrypt_flags = (gpgme_encrypt_flags_t) flags;
	gpgme_data_t input, output;
	gpgme_error_t error;
	gpgme_key_t *rcpts;
	gpgme_key_t key;
	guint i;
	
	/* create an array of recipient keys for GpgMe */
	rcpts = g_new0 (gpgme_key_t, recipients->len + 1);
	for (i = 0; i < recipients->len; i++) {
		if (!(key = g_mime_gpgme_get_key_by_name (ctx, recipients->pdata[i], FALSE, err))) {
			g_mime_gpgme_keylist_free (rcpts);
			return -1;
		}
		
		rcpts[i] = key;
	}
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
			     gpgme_strerror (error));
		g_mime_gpgme_keylist_free (rcpts);
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream: %s"),
			     gpgme_strerror (error));
		g_mime_gpgme_keylist_free (rcpts);
		gpgme_data_release (input);
		return -1;
	}
	
	/* encrypt the input stream */
	if (sign) {
		if (!g_mime_gpgme_add_signer (ctx, userid, err)) {
			g_mime_gpgme_keylist_free (rcpts);
			gpgme_data_release (output);
			gpgme_data_release (input);
			return -1;
		}
		
		error = gpgme_op_encrypt_sign (ctx, rcpts, encrypt_flags, input, output);
		
		gpgme_signers_clear (ctx);
	} else {
		error = gpgme_op_encrypt (ctx, rcpts, encrypt_flags, input, output);
	}
	
	g_mime_gpgme_keylist_free (rcpts);
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Encryption failed: %s"), gpgme_strerror (error));
		return -1;
	}
	
	return 0;
}

static GMimeDecryptResult *
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
	
#if GPGME_VERSION_NUMBER >= 0x010800
	if (res->session_key)
		result->session_key = g_strdup (res->session_key);
#endif
	
	recipient = res->recipients;
	while (recipient != NULL) {
		cert = g_mime_certificate_new ();
		g_mime_certificate_list_add (result->recipients, cert);
		
		g_mime_certificate_set_pubkey_algo (cert, (GMimePubKeyAlgo) recipient->pubkey_algo);
		g_mime_certificate_set_key_id (cert, recipient->keyid);
		g_object_unref(cert);
		
		recipient = recipient->next;
	}
	
	return result;
}

GMimeDecryptResult *
g_mime_gpgme_decrypt (gpgme_ctx_t ctx, GMimeDecryptFlags flags, const char *session_key,
		      GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
			     gpgme_strerror (error));
		return NULL;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream: %s"),
			     gpgme_strerror (error));
		gpgme_data_release (input);
		return NULL;
	}
	
#if GPGME_VERSION_NUMBER >= 0x010800
	if (flags & GMIME_DECRYPT_EXPORT_SESSION_KEY)
		gpgme_set_ctx_flag (ctx, "export-session-key", "1");
	
	if (session_key)
		gpgme_set_ctx_flag (ctx, "override-session-key", session_key);
#endif
	
	/* decrypt the input stream */
	if (gpgme_get_protocol (ctx) == GPGME_PROTOCOL_OpenPGP && (flags & GMIME_DECRYPT_NO_VERIFY) == 0) {
		gpgme_set_offline (ctx, (flags & GMIME_DECRYPT_ENABLE_KEYSERVER_LOOKUPS) == 0);
		
		error = gpgme_op_decrypt_verify (ctx, input, output);
	} else {
		error = gpgme_op_decrypt (ctx, input, output);
	}
	
#if GPGME_VERSION_NUMBER >= 0x010800
	if (flags & GMIME_DECRYPT_EXPORT_SESSION_KEY)
		gpgme_set_ctx_flag (ctx, "export-session-key", "0");
	
	if (session_key)
		gpgme_set_ctx_flag (ctx, "override-session-key", NULL);
#endif
	
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Decryption failed: %s"), gpgme_strerror (error));
		return NULL;
	}
	
	return g_mime_gpgme_get_decrypt_result (ctx);
}

int
g_mime_gpgme_import (gpgme_ctx_t ctx, GMimeStream *istream, GError **err)
{
	gpgme_import_result_t result;
	gpgme_data_t keydata;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream: %s"),
			     gpgme_strerror (error));
		return -1;
	}
	
	/* import the key(s) */
	error = gpgme_op_import (ctx, keydata);
	gpgme_data_release (keydata);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not import key data: %s"),
			     gpgme_strerror (error));
		return -1;
	}
	
	result = gpgme_op_import_result (ctx);
	
	return result->imported;
}

int
g_mime_gpgme_export (gpgme_ctx_t ctx, const char *keys[], GMimeStream *ostream, GError **err)
{
	gpgme_data_t keydata;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream: %s"),
			     gpgme_strerror (error));
		return -1;
	}
	
	/* export the key(s) */
	error = gpgme_op_export_ext (ctx, keys, 0, keydata);
	gpgme_data_release (keydata);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not export key data: %s"),
			     gpgme_strerror (error));
		return -1;
	}
	
	return 0;
}

#endif /* ENABLE_CRYPTO */
