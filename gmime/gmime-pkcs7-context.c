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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gpgme.h>

#include "gmime-pkcs7-context.h"
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-pipe.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#include "gmime-error.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


/**
 * SECTION: gmime-pkcs7-context
 * @title: GMimePkcs7Context
 * @short_description: PKCS7 cipher contexts
 * @see_also: #GMimeCipherContext
 *
 * A #GMimePkcs7Context is a #GMimeCipherContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/

typedef struct _GMimePkcs7ContextPrivate {
	gboolean always_trust;
	gpgme_ctx_t ctx;
} Pkcs7Ctx;

static void g_mime_pkcs7_context_class_init (GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_init (GMimePkcs7Context *ctx, GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_finalize (GObject *object);

static GMimeCipherHash pkcs7_hash_id (GMimeCipherContext *ctx, const char *hash);

static const char *pkcs7_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

static int pkcs7_sign (GMimeCipherContext *ctx, const char *userid,
		       GMimeCipherHash hash, GMimeStream *istream,
		       GMimeStream *ostream, GError **err);
	
static GMimeSignatureValidity *pkcs7_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					     GMimeStream *istream, GMimeStream *sigstream,
					     GError **err);

static int pkcs7_encrypt (GMimeCipherContext *ctx, gboolean sign,
			  const char *userid, GPtrArray *recipients,
			  GMimeStream *istream, GMimeStream *ostream,
			  GError **err);

static GMimeSignatureValidity *pkcs7_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
					      GMimeStream *ostream, GError **err);

static int pkcs7_import_keys (GMimeCipherContext *ctx, GMimeStream *istream,
			      GError **err);

static int pkcs7_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
			      GMimeStream *ostream, GError **err);


static GMimeCipherContextClass *parent_class = NULL;


GType
g_mime_pkcs7_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimePkcs7ContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_pkcs7_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimePkcs7Context),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_pkcs7_context_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_CIPHER_CONTEXT, "GMimePkcs7Context", &info, 0);
	}
	
	return type;
}


static void
g_mime_pkcs7_context_class_init (GMimePkcs7ContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeCipherContextClass *cipher_class = GMIME_CIPHER_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_pkcs7_context_finalize;
	
	cipher_class->hash_id = pkcs7_hash_id;
	cipher_class->hash_name = pkcs7_hash_name;
	cipher_class->sign = pkcs7_sign;
	cipher_class->verify = pkcs7_verify;
	cipher_class->encrypt = pkcs7_encrypt;
	cipher_class->decrypt = pkcs7_decrypt;
	cipher_class->import_keys = pkcs7_import_keys;
	cipher_class->export_keys = pkcs7_export_keys;
}

static void
g_mime_pkcs7_context_init (GMimePkcs7Context *ctx, GMimePkcs7ContextClass *klass)
{
	GMimeCipherContext *cipher = (GMimeCipherContext *) ctx;
	
	ctx->priv = g_slice_new (Pkcs7Ctx);
	ctx->priv->always_trust = FALSE;
	ctx->priv->ctx = NULL;
	
	cipher->sign_protocol = "application/pkcs7-signature";
	cipher->encrypt_protocol = "application/pkcs7-mime";
	cipher->key_protocol = "application/pkcs7-keys";
}

static void
g_mime_pkcs7_context_finalize (GObject *object)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) object;
	
	if (ctx->priv->ctx)
		gpgme_release (ctx->priv->ctx);
	
	g_slice_free (Pkcs7Ctx, ctx->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GMimeCipherHash
pkcs7_hash_id (GMimeCipherContext *ctx, const char *hash)
{
	if (hash == NULL)
		return GMIME_CIPHER_HASH_DEFAULT;
	
	if (!g_ascii_strcasecmp (hash, "md2"))
		return GMIME_CIPHER_HASH_MD2;
	else if (!g_ascii_strcasecmp (hash, "md5"))
		return GMIME_CIPHER_HASH_MD5;
	else if (!g_ascii_strcasecmp (hash, "sha1"))
		return GMIME_CIPHER_HASH_SHA1;
	else if (!g_ascii_strcasecmp (hash, "sha224"))
		return GMIME_CIPHER_HASH_SHA224;
	else if (!g_ascii_strcasecmp (hash, "sha256"))
		return GMIME_CIPHER_HASH_SHA256;
	else if (!g_ascii_strcasecmp (hash, "sha384"))
		return GMIME_CIPHER_HASH_SHA384;
	else if (!g_ascii_strcasecmp (hash, "sha512"))
		return GMIME_CIPHER_HASH_SHA512;
	else if (!g_ascii_strcasecmp (hash, "ripemd160"))
		return GMIME_CIPHER_HASH_RIPEMD160;
	else if (!g_ascii_strcasecmp (hash, "tiger192"))
		return GMIME_CIPHER_HASH_TIGER192;
	else if (!g_ascii_strcasecmp (hash, "haval-5-160"))
		return GMIME_CIPHER_HASH_HAVAL5160;
	
	return GMIME_CIPHER_HASH_DEFAULT;
}

static const char *
pkcs7_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash)
{
	switch (hash) {
	case GMIME_CIPHER_HASH_MD2:
		return "md2";
	case GMIME_CIPHER_HASH_MD5:
		return "md5";
	case GMIME_CIPHER_HASH_SHA1:
		return "sha1";
	case GMIME_CIPHER_HASH_SHA224:
		return "sha224";
	case GMIME_CIPHER_HASH_SHA256:
		return "sha256";
	case GMIME_CIPHER_HASH_SHA384:
		return "sha384";
	case GMIME_CIPHER_HASH_SHA512:
		return "sha512";
	case GMIME_CIPHER_HASH_RIPEMD160:
		return "ripemd160";
	case GMIME_CIPHER_HASH_TIGER192:
		return "tiger192";
	case GMIME_CIPHER_HASH_HAVAL5160:
		return "haval-5-160";
	default:
		return "sha1";
	}
}

static gpgme_error_t
pkcs7_passphrase_cb (void *hook, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd)
{
	GMimeCipherContext *context = (GMimeCipherContext *) hook;
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

static ssize_t
pkcs7_stream_read (void *stream, void *buffer, size_t size)
{
	return g_mime_stream_read ((GMimeStream *) stream, (char *) buffer, size);
}

static ssize_t
pkcs7_stream_write (void *stream, const void *buffer, size_t size)
{
	return g_mime_stream_write ((GMimeStream *) stream, (const char *) buffer, size);
}

static void
pkcs7_stream_free (void *stream)
{
	/* no-op */
}

static struct gpgme_data_cbs pkcs7_stream_funcs = {
	pkcs7_stream_read,
	pkcs7_stream_write,
	NULL,
	pkcs7_stream_free
};



#define KEY_IS_OK(k)   (!((k)->expired || (k)->revoked ||	\
                          (k)->disabled || (k)->invalid))

static gpgme_key_t
pkcs7_get_key_by_name (Pkcs7Ctx *pkcs7, const char *name, gboolean secret, GError **err)
{
	time_t now = time (NULL);
	gpgme_key_t key = NULL;
	gpgme_subkey_t subkey;
	gboolean bad = FALSE;
	gpgme_error_t error;
	int errval = 0;
	
	if ((error = gpgme_op_keylist_start (pkcs7->ctx, name, secret)) != GPG_ERR_NO_ERROR) {
		if (secret)
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list secret keys for \"%s\""), name);
		else
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list keys for \"%s\""), name);
		return NULL;
	}
	
	while ((error = gpgme_op_keylist_next (pkcs7->ctx, &key)) == GPG_ERR_NO_ERROR) {
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
	
	gpgme_op_keylist_end (pkcs7->ctx);
	
	if (error != GPG_ERR_NO_ERROR && error != GPG_ERR_EOF) {
		if (secret)
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list secret keys for \"%s\""), name);
		else
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list keys for \"%s\""), name);
		
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

static gboolean
pkcs7_add_signer (Pkcs7Ctx *pkcs7, const char *signer, GError **err)
{
	gpgme_key_t key = NULL;
	
	if (!(key = pkcs7_get_key_by_name (pkcs7, signer, TRUE, err)))
		return FALSE;
	
	/* set the key (the previous operation guaranteed that it exists, no need
	 * 2 check return values...) */
	gpgme_signers_add (pkcs7->ctx, key);
	gpgme_key_unref (key);
	
	return TRUE;
}

static int
pkcs7_sign (GMimeCipherContext *context, const char *userid, GMimeCipherHash hash,
	    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_sign_result_t result;
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	if (!pkcs7_add_signer (pkcs7, userid, err))
		return -1;
	
	gpgme_set_armor (pkcs7->ctx, FALSE);
	
	if ((error = gpgme_data_new_from_cbs (&input, &pkcs7_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &pkcs7_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_data_release (input);
		return -1;
	}
	
	/* sign the input stream */
	if ((error = gpgme_op_sign (pkcs7->ctx, input, output, GPGME_SIG_MODE_DETACH)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Signing failed"));
		gpgme_data_release (output);
		gpgme_data_release (input);
		return -1;
	}
	
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	/* return the hash algorithm used for signing */
	result = gpgme_op_sign_result (pkcs7->ctx);
	
	return pkcs7_hash_id (context, gpgme_hash_algo_name (result->signatures->hash_algo));
}

static GMimeSignerTrust
pkcs7_trust (gpgme_validity_t trust)
{
	switch (trust) {
	case GPGME_VALIDITY_UNKNOWN:
		return GMIME_SIGNER_TRUST_NONE;
	case GPGME_VALIDITY_UNDEFINED:
		return GMIME_SIGNER_TRUST_UNDEFINED;
	case GPGME_VALIDITY_NEVER:
		return GMIME_SIGNER_TRUST_NEVER;
	case GPGME_VALIDITY_MARGINAL:
		return GMIME_SIGNER_TRUST_MARGINAL;
	case GPGME_VALIDITY_FULL:
		return GMIME_SIGNER_TRUST_FULLY;
	case GPGME_VALIDITY_ULTIMATE:
		return GMIME_SIGNER_TRUST_ULTIMATE;
	}
}

static GMimeSignatureValidity *
pkcs7_get_validity (Pkcs7Ctx *pkcs7, gboolean verify)
{
	GMimeSignatureStatus status = GMIME_SIGNATURE_STATUS_GOOD;
	GMimeSignatureValidity *validity;
	GMimeSigner *signers, *signer;
	gpgme_verify_result_t result;
	GMimeSignerError errors;
	gpgme_subkey_t subkey;
	gpgme_signature_t sig;
	gpgme_user_id_t uid;
	gpgme_key_t key;
	
	/* create a new signature validity to return */
	validity = g_mime_signature_validity_new ();
	
	/* get the signature verification results from GpgMe */
	if (!(result = gpgme_op_verify_result (pkcs7->ctx)) || !result->signatures) {
		if (verify)
			g_mime_signature_validity_set_status (validity, GMIME_SIGNATURE_STATUS_UNKNOWN);
		
		return validity;
	}
	
	/* collect the signers for this signature */
	signers = (GMimeSigner *) &validity->signers;
	sig = result->signatures;
	
	while (sig != NULL) {
		signer = g_mime_signer_new ();
		signers->next = signer;
		signers = signer;
		
		g_mime_signer_set_sig_expires (signer, sig->exp_timestamp);
		g_mime_signer_set_sig_created (signer, sig->timestamp);
		g_mime_signer_set_fingerprint (signer, sig->fpr);
		
		errors = GMIME_SIGNER_ERROR_NONE;
		
		if (sig->exp_timestamp != 0 && sig->exp_timestamp >= time (NULL))
			errors |= GMIME_SIGNER_ERROR_EXPSIG;
		
		if (gpgme_get_key (pkcs7->ctx, sig->fpr, &key, 0) == GPG_ERR_NO_ERROR && key) {
			/* get more signer info from their signing key */
			g_mime_signer_set_trust (signer, pkcs7_trust (key->owner_trust));
			g_mime_signer_set_issuer_serial (signer, key->issuer_serial);
			g_mime_signer_set_issuer_name (signer, key->issuer_name);
			
			/* get the name and email address */
			uid = key->uids;
			while (uid) {
				if (uid->name && *uid->name)
					g_mime_signer_set_name (signer, uid->name);
				
				if (uid->email && *uid->email)
					g_mime_signer_set_email (signer, uid->email);
				
				if (uid->uid && *uid->uid)
					g_mime_signer_set_key_id (signer, uid->uid);
				
				if (signer->name && signer->email && signer->keyid)
					break;
				
				uid = uid->next;
			}
			
			/* get the subkey used for signing */
			subkey = key->subkeys;
			while (subkey && !subkey->can_sign)
				subkey = subkey->next;
			
			if (subkey) {
				g_mime_signer_set_key_created (signer, subkey->timestamp);
				g_mime_signer_set_key_expires (signer, subkey->expires);
				
				if (subkey->revoked)
					errors |= GMIME_SIGNER_ERROR_REVKEYSIG;
				
				if (subkey->expired)
					errors |= GMIME_SIGNER_ERROR_EXPKEYSIG;
			} else {
				errors |= GMIME_SIGNER_ERROR_NO_PUBKEY;
			}
			
			gpgme_key_unref (key);
		} else {
			/* don't have any key information available... */
			g_mime_signer_set_trust (signer, GMIME_SIGNER_TRUST_UNDEFINED);
			errors |= GMIME_SIGNER_ERROR_NO_PUBKEY;
		}
		
		/* set the accumulated signer errors */
		g_mime_signer_set_errors (signer, errors);
		
		/* get the signer's status and update overall status */
		if (sig->status != GPG_ERR_NO_ERROR) {
			if (signer->errors && signer->errors != GMIME_SIGNER_ERROR_NO_PUBKEY) {
				g_mime_signer_set_status (signer, GMIME_SIGNER_STATUS_ERROR);
				if (status != GMIME_SIGNATURE_STATUS_BAD)
					status = GMIME_SIGNATURE_STATUS_UNKNOWN;
			} else {
				g_mime_signer_set_status (signer, GMIME_SIGNER_STATUS_BAD);
				status = GMIME_SIGNATURE_STATUS_BAD;
			}
		} else {
			g_mime_signer_set_status (signer, GMIME_SIGNER_STATUS_GOOD);
		}
		
		sig = sig->next;
	}
	
	/* set the resulting overall signature status */
	g_mime_signature_validity_set_status (validity, status);
	
	return validity;
}

static GMimeSignatureValidity *
pkcs7_verify (GMimeCipherContext *context, GMimeCipherHash hash,
	      GMimeStream *istream, GMimeStream *sigstream,
	      GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	gpgme_data_t message, signature;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&message, &pkcs7_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return NULL;
	}
	
	/* if @sigstream is non-NULL, then it is a detached signature */
	if (sigstream != NULL) {
		if ((error = gpgme_data_new_from_cbs (&signature, &pkcs7_stream_funcs, sigstream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open signature stream"));
			gpgme_data_release (message);
			return NULL;
		}
	} else {
		signature = NULL;
	}
	
	if ((error = gpgme_op_verify (pkcs7->ctx, signature, message, NULL)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not verify pkcs7 signature"));
		if (signature)
			gpgme_data_release (signature);
		gpgme_data_release (message);
		return NULL;
	}
	
	if (signature)
		gpgme_data_release (signature);
	
	if (message)
		gpgme_data_release (message);
	
	/* get/return the pkcs7 signature validity */
	return pkcs7_get_validity (pkcs7, TRUE);
}

static void
key_list_free (gpgme_key_t *keys)
{
	gpgme_key_t *key = keys;
	
	while (key != NULL) {
		gpgme_key_unref (*key);
		key++;
	}
	
	g_free (keys);
}

static int
pkcs7_encrypt (GMimeCipherContext *context, gboolean sign, const char *userid,
	       GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream,
	       GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_data_t input, output;
	gpgme_error_t error;
	gpgme_key_t *rcpts;
	gpgme_key_t key;
	guint i;
	
	if (sign) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("Cannot sign and encrypt a stream at the same time using pkcs7"));
		return -1;
	}
	
	/* create an array of recipient keys for GpgMe */
	rcpts = g_new0 (gpgme_key_t, recipients->len + 1);
	for (i = 0; i < recipients->len; i++) {
		if (!(key = pkcs7_get_key_by_name (pkcs7, recipients->pdata[i], FALSE, err))) {
			key_list_free (rcpts);
			return -1;
		}
		
		rcpts[i] = key;
	}
	
	if ((error = gpgme_data_new_from_cbs (&input, &pkcs7_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		key_list_free (rcpts);
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &pkcs7_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_data_release (input);
		key_list_free (rcpts);
		return -1;
	}
	
	/* encrypt the input stream */
	error = gpgme_op_encrypt (pkcs7->ctx, rcpts, GPGME_ENCRYPT_ALWAYS_TRUST, input, output);
	gpgme_data_release (output);
	gpgme_data_release (input);
	key_list_free (rcpts);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Encryption failed"));
		return -1;
	}
	
	return 0;
}


static GMimeSignatureValidity *
pkcs7_decrypt (GMimeCipherContext *context, GMimeStream *istream,
	       GMimeStream *ostream, GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&input, &pkcs7_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return NULL;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &pkcs7_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_data_release (input);
		return NULL;
	}
	
	/* decrypt the input stream */
	if ((error = gpgme_op_decrypt_verify (pkcs7->ctx, input, output)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Decryption failed"));
		gpgme_data_release (output);
		gpgme_data_release (input);
		return NULL;
	}
	
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	/* get/return the pkcs7 signature validity */
	return pkcs7_get_validity (pkcs7, FALSE);
}

static int
pkcs7_import_keys (GMimeCipherContext *context, GMimeStream *istream, GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_data_t keydata;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &pkcs7_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return -1;
	}
	
	/* import the key(s) */
	if ((error = gpgme_op_import (pkcs7->ctx, keydata)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not import key data"));
		gpgme_data_release (keydata);
		return -1;
	}
	
	gpgme_data_release (keydata);
	
	return 0;
}

static int
pkcs7_export_keys (GMimeCipherContext *context, GPtrArray *keys, GMimeStream *ostream, GError **err)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_data_t keydata;
	gpgme_error_t error;
	guint i;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &pkcs7_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		return -1;
	}
	
	/* export the key(s) */
	for (i = 0; i < keys->len; i++) {
		if ((error = gpgme_op_export (pkcs7->ctx, keys->pdata[i], 0, keydata)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not export key data"));
			gpgme_data_release (keydata);
			return -1;
		}
	}
	
	gpgme_data_release (keydata);
	
	return 0;
}


/**
 * g_mime_pkcs7_context_new:
 * @request_passwd: a #GMimePasswordRequestFunc
 *
 * Creates a new pkcs7 cipher context object.
 *
 * Returns: a new pkcs7 cipher context object.
 **/
GMimeCipherContext *
g_mime_pkcs7_context_new (GMimePasswordRequestFunc request_passwd)
{
	GMimeCipherContext *cipher;
	GMimePkcs7Context *pkcs7;
	gpgme_ctx_t ctx;
	
	/* make sure GpgMe supports the CMS protocols */
	if (gpgme_engine_check_version (GPGME_PROTOCOL_CMS) != GPG_ERR_NO_ERROR)
		return NULL;
	
	/* create the GpgMe context */
	if (gpgme_new (&ctx) != GPG_ERR_NO_ERROR)
		return NULL;
	
	pkcs7 = g_object_newv (GMIME_TYPE_PKCS7_CONTEXT, 0, NULL);
	gpgme_set_passphrase_cb (ctx, pkcs7_passphrase_cb, pkcs7);
	gpgme_set_protocol (ctx, GPGME_PROTOCOL_CMS);
	pkcs7->priv->ctx = ctx;
	
	cipher = (GMimeCipherContext *) pkcs7;
	cipher->request_passwd = request_passwd;
	
	return cipher;
}


/**
 * g_mime_pkcs7_context_get_always_trust:
 * @ctx: a #GMimePkcs7Context
 *
 * Gets the @always_trust flag on the pkcs7 context.
 *
 * Returns: the @always_trust flag on the pkcs7 context.
 **/
gboolean
g_mime_pkcs7_context_get_always_trust (GMimePkcs7Context *ctx)
{
	g_return_val_if_fail (GMIME_IS_PKCS7_CONTEXT (ctx), FALSE);
	
	return ctx->priv->always_trust;
}


/**
 * g_mime_pkcs7_context_set_always_trust:
 * @ctx: a #GMimePkcs7Context
 * @always_trust: always trust flag
 *
 * Sets the @always_trust flag on the pkcs7 context which is used for
 * encryption.
 **/
void
g_mime_pkcs7_context_set_always_trust (GMimePkcs7Context *ctx, gboolean always_trust)
{
	g_return_if_fail (GMIME_IS_PKCS7_CONTEXT (ctx));
	
	ctx->priv->always_trust = always_trust;
}
