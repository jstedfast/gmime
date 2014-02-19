/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include "gmime-pkcs7-context.h"
#ifdef ENABLE_SMIME
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-pipe.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#endif /* ENABLE_SMIME */
#include "gmime-error.h"

#ifdef ENABLE_SMIME
#include <gpgme.h>
#endif

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


/**
 * SECTION: gmime-pkcs7-context
 * @title: GMimePkcs7Context
 * @short_description: PKCS7 crypto contexts
 * @see_also: #GMimeCryptoContext
 *
 * A #GMimePkcs7Context is a #GMimeCryptoContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/

typedef struct _GMimePkcs7ContextPrivate {
	gboolean always_trust;
#ifdef ENABLE_SMIME
	gpgme_ctx_t ctx;
#endif
} Pkcs7Ctx;

static void g_mime_pkcs7_context_class_init (GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_init (GMimePkcs7Context *ctx, GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_finalize (GObject *object);

static GMimeDigestAlgo pkcs7_digest_id (GMimeCryptoContext *ctx, const char *name);

static const char *pkcs7_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);

static const char *pkcs7_get_signature_protocol (GMimeCryptoContext *ctx);

static const char *pkcs7_get_encryption_protocol (GMimeCryptoContext *ctx);

static const char *pkcs7_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static int pkcs7_sign (GMimeCryptoContext *ctx, const char *userid,
		       GMimeDigestAlgo digest, GMimeStream *istream,
		       GMimeStream *ostream, GError **err);
	
static GMimeSignatureList *pkcs7_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
					 GMimeStream *istream, GMimeStream *sigstream,
					 GError **err);

static int pkcs7_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid,
			  GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
			  GMimeStream *ostream, GError **err);

static GMimeDecryptResult *pkcs7_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
					  GMimeStream *ostream, GError **err);

static int pkcs7_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream,
			      GError **err);

static int pkcs7_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
			      GMimeStream *ostream, GError **err);


static GMimeCryptoContextClass *parent_class = NULL;


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
		
		type = g_type_register_static (GMIME_TYPE_CRYPTO_CONTEXT, "GMimePkcs7Context", &info, 0);
	}
	
	return type;
}


static void
g_mime_pkcs7_context_class_init (GMimePkcs7ContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeCryptoContextClass *crypto_class = GMIME_CRYPTO_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_pkcs7_context_finalize;
	
	crypto_class->digest_id = pkcs7_digest_id;
	crypto_class->digest_name = pkcs7_digest_name;
	crypto_class->sign = pkcs7_sign;
	crypto_class->verify = pkcs7_verify;
	crypto_class->encrypt = pkcs7_encrypt;
	crypto_class->decrypt = pkcs7_decrypt;
	crypto_class->import_keys = pkcs7_import_keys;
	crypto_class->export_keys = pkcs7_export_keys;
	crypto_class->get_signature_protocol = pkcs7_get_signature_protocol;
	crypto_class->get_encryption_protocol = pkcs7_get_encryption_protocol;
	crypto_class->get_key_exchange_protocol = pkcs7_get_key_exchange_protocol;
}

static void
g_mime_pkcs7_context_init (GMimePkcs7Context *ctx, GMimePkcs7ContextClass *klass)
{
	ctx->priv = g_slice_new (Pkcs7Ctx);
	ctx->priv->always_trust = FALSE;
#ifdef ENABLE_SMIME
	ctx->priv->ctx = NULL;
#endif
}

static void
g_mime_pkcs7_context_finalize (GObject *object)
{
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) object;
	
#ifdef ENABLE_SMIME
	if (ctx->priv->ctx)
		gpgme_release (ctx->priv->ctx);
#endif
	
	g_slice_free (Pkcs7Ctx, ctx->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GMimeDigestAlgo
pkcs7_digest_id (GMimeCryptoContext *ctx, const char *name)
{
	if (name == NULL)
		return GMIME_DIGEST_ALGO_DEFAULT;
	
	if (!g_ascii_strcasecmp (name, "md2"))
		return GMIME_DIGEST_ALGO_MD2;
	else if (!g_ascii_strcasecmp (name, "md4"))
		return GMIME_DIGEST_ALGO_MD4;
	else if (!g_ascii_strcasecmp (name, "md5"))
		return GMIME_DIGEST_ALGO_MD5;
	else if (!g_ascii_strcasecmp (name, "sha1"))
		return GMIME_DIGEST_ALGO_SHA1;
	else if (!g_ascii_strcasecmp (name, "sha224"))
		return GMIME_DIGEST_ALGO_SHA224;
	else if (!g_ascii_strcasecmp (name, "sha256"))
		return GMIME_DIGEST_ALGO_SHA256;
	else if (!g_ascii_strcasecmp (name, "sha384"))
		return GMIME_DIGEST_ALGO_SHA384;
	else if (!g_ascii_strcasecmp (name, "sha512"))
		return GMIME_DIGEST_ALGO_SHA512;
	else if (!g_ascii_strcasecmp (name, "ripemd160"))
		return GMIME_DIGEST_ALGO_RIPEMD160;
	else if (!g_ascii_strcasecmp (name, "tiger192"))
		return GMIME_DIGEST_ALGO_TIGER192;
	else if (!g_ascii_strcasecmp (name, "haval-5-160"))
		return GMIME_DIGEST_ALGO_HAVAL5160;
	
	return GMIME_DIGEST_ALGO_DEFAULT;
}

static const char *
pkcs7_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest)
{
	switch (digest) {
	case GMIME_DIGEST_ALGO_MD2:
		return "md2";
	case GMIME_DIGEST_ALGO_MD4:
		return "md4";
	case GMIME_DIGEST_ALGO_MD5:
		return "md5";
	case GMIME_DIGEST_ALGO_SHA1:
		return "sha1";
	case GMIME_DIGEST_ALGO_SHA224:
		return "sha224";
	case GMIME_DIGEST_ALGO_SHA256:
		return "sha256";
	case GMIME_DIGEST_ALGO_SHA384:
		return "sha384";
	case GMIME_DIGEST_ALGO_SHA512:
		return "sha512";
	case GMIME_DIGEST_ALGO_RIPEMD160:
		return "ripemd160";
	case GMIME_DIGEST_ALGO_TIGER192:
		return "tiger192";
	case GMIME_DIGEST_ALGO_HAVAL5160:
		return "haval-5-160";
	default:
		return "sha1";
	}
}

static const char *
pkcs7_get_signature_protocol (GMimeCryptoContext *ctx)
{
	return "application/pkcs7-signature";
}

static const char *
pkcs7_get_encryption_protocol (GMimeCryptoContext *ctx)
{
	return "application/pkcs7-mime";
}

static const char *
pkcs7_get_key_exchange_protocol (GMimeCryptoContext *ctx)
{
	return "application/pkcs7-keys";
}

#ifdef ENABLE_SMIME
static gpgme_error_t
pkcs7_passphrase_cb (void *hook, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd)
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

static off_t
pkcs7_stream_seek (void *stream, off_t offset, int whence)
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
pkcs7_stream_free (void *stream)
{
	/* no-op */
}

static struct gpgme_data_cbs pkcs7_stream_funcs = {
	pkcs7_stream_read,
	pkcs7_stream_write,
	pkcs7_stream_seek,
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
#endif /* ENABLE_SMIME */

static int
pkcs7_sign (GMimeCryptoContext *context, const char *userid, GMimeDigestAlgo digest,
	    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_SMIME
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
	
	/* return the digest algorithm used for signing */
	result = gpgme_op_sign_result (pkcs7->ctx);
	
	return pkcs7_digest_id (context, gpgme_hash_algo_name (result->signatures->hash_algo));
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_SMIME */
}

#ifdef ENABLE_SMIME
static GMimeCertificateTrust
pkcs7_trust (gpgme_validity_t trust)
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

static GMimeSignatureList *
pkcs7_get_signatures (Pkcs7Ctx *pkcs7, gboolean verify)
{
	GMimeSignatureList *signatures;
	GMimeSignature *signature;
	gpgme_verify_result_t result;
	gpgme_subkey_t subkey;
	gpgme_signature_t sig;
	gpgme_user_id_t uid;
	gpgme_key_t key;
	
	/* get the signature verification results from GpgMe */
	if (!(result = gpgme_op_verify_result (pkcs7->ctx)) || !result->signatures)
		return verify ? g_mime_signature_list_new () : NULL;
	
	/* create a new signature list to return */
	signatures = g_mime_signature_list_new ();
	
	sig = result->signatures;
	
	while (sig != NULL) {
		signature = g_mime_signature_new ();
		g_mime_signature_list_add (signatures, signature);
		
		if (sig->status != GPG_ERR_NO_ERROR)
			g_mime_signature_set_status (signature, GMIME_SIGNATURE_STATUS_ERROR);
		else
			g_mime_signature_set_status (signature, GMIME_SIGNATURE_STATUS_GOOD);
		
		g_mime_certificate_set_pubkey_algo (signature->cert, sig->pubkey_algo);
		g_mime_certificate_set_digest_algo (signature->cert, sig->hash_algo);
		g_mime_certificate_set_fingerprint (signature->cert, sig->fpr);
		g_mime_signature_set_expires (signature, sig->exp_timestamp);
		g_mime_signature_set_created (signature, sig->timestamp);
		
		if (sig->exp_timestamp != 0 && sig->exp_timestamp <= time (NULL)) {
			/* signature expired, automatically results in a BAD signature */
			signature->errors |= GMIME_SIGNATURE_ERROR_EXPSIG;
			signature->status = GMIME_SIGNATURE_STATUS_BAD;
		}
		
		if (gpgme_get_key (pkcs7->ctx, sig->fpr, &key, 0) == GPG_ERR_NO_ERROR && key) {
			/* get more signer info from their signing key */
			g_mime_certificate_set_trust (signature->cert, pkcs7_trust (key->owner_trust));
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
				
				if (subkey->revoked) {
					/* signer's key has been revoked, automatic BAD status */
					signature->errors |= GMIME_SIGNATURE_ERROR_REVKEYSIG;
					signature->status = GMIME_SIGNATURE_STATUS_BAD;
				}
				
				if (subkey->expired) {
					/* signer's key has expired, automatic BAD status */
					signature->errors |= GMIME_SIGNATURE_ERROR_EXPKEYSIG;
					signature->status = GMIME_SIGNATURE_STATUS_BAD;
				}
			} else {
				/* If we don't have the subkey used by the signer, then we can't
				 * tell what the status is, so set to ERROR if it hasn't already
				 * been designated as BAD. */
				if (signature->status != GMIME_SIGNATURE_STATUS_BAD)
					signature->status = GMIME_SIGNATURE_STATUS_ERROR;
				signature->errors |= GMIME_SIGNATURE_ERROR_NO_PUBKEY;
			}
			
			gpgme_key_unref (key);
		} else {
			/* If we don't have the signer's public key, then we can't tell what
			 * the status is, so set it to ERROR if it hasn't already been
			 * designated as BAD. */
			g_mime_certificate_set_trust (signature->cert, GMIME_CERTIFICATE_TRUST_UNDEFINED);
			if (signature->status != GMIME_SIGNATURE_STATUS_BAD)
				signature->status = GMIME_SIGNATURE_STATUS_ERROR;
			signature->errors |= GMIME_SIGNATURE_ERROR_NO_PUBKEY;
		}
		
		sig = sig->next;
	}
	
	return signatures;
}
#endif /* ENABLE_SMIME */

static GMimeSignatureList *
pkcs7_verify (GMimeCryptoContext *context, GMimeDigestAlgo digest,
	      GMimeStream *istream, GMimeStream *sigstream,
	      GError **err)
{
#ifdef ENABLE_SMIME
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
	
	/* get/return the pkcs7 signatures */
	return pkcs7_get_signatures (pkcs7, TRUE);
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_SMIME */
}

#ifdef ENABLE_SMIME
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
#endif /* ENABLE_SMIME */

static int
pkcs7_encrypt (GMimeCryptoContext *context, gboolean sign, const char *userid,
	       GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
	       GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_SMIME
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
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_SMIME */
}

#ifdef ENABLE_SMIME
static GMimeDecryptResult *
pkcs7_get_decrypt_result (Pkcs7Ctx *pkcs7)
{
	GMimeDecryptResult *result;
	gpgme_decrypt_result_t res;
	gpgme_recipient_t recipient;
	GMimeCertificate *cert;
	
	result = g_mime_decrypt_result_new ();
	result->recipients = g_mime_certificate_list_new ();
	result->signatures = pkcs7_get_signatures (pkcs7, FALSE);
	
	if (!(res = gpgme_op_decrypt_result (pkcs7->ctx)) || !res->recipients)
		return result;
	
	recipient = res->recipients;
	while (recipient != NULL) {
		cert = g_mime_certificate_new ();
		g_mime_certificate_list_add (result->recipients, cert);
		
		g_mime_certificate_set_pubkey_algo (cert, recipient->pubkey_algo);
		g_mime_certificate_set_key_id (cert, recipient->keyid);
		
		recipient = recipient->next;
	}
	
	return result;
}
#endif /* ENABLE_SMIME */

static GMimeDecryptResult *
pkcs7_decrypt (GMimeCryptoContext *context, GMimeStream *istream,
	       GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_SMIME
	GMimePkcs7Context *ctx = (GMimePkcs7Context *) context;
	GMimeDecryptResult *result;
	Pkcs7Ctx *pkcs7 = ctx->priv;
	gpgme_decrypt_result_t res;
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
	
	return pkcs7_get_decrypt_result (pkcs7);
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_SMIME */
}

static int
pkcs7_import_keys (GMimeCryptoContext *context, GMimeStream *istream, GError **err)
{
#ifdef ENABLE_SMIME
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
		//printf ("import error (%d): %s\n", error & GPG_ERR_CODE_MASK, gpg_strerror (error));
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not import key data"));
		gpgme_data_release (keydata);
		return -1;
	}
	
	gpgme_data_release (keydata);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_SMIME */
}

static int
pkcs7_export_keys (GMimeCryptoContext *context, GPtrArray *keys, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_SMIME
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
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_SMIME */
}


/**
 * g_mime_pkcs7_context_new:
 * @request_passwd: a #GMimePasswordRequestFunc
 *
 * Creates a new pkcs7 crypto context object.
 *
 * Returns: (transfer full): a new pkcs7 crypto context object.
 **/
GMimeCryptoContext *
g_mime_pkcs7_context_new (GMimePasswordRequestFunc request_passwd)
{
#ifdef ENABLE_SMIME
	GMimeCryptoContext *crypto;
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
	
	crypto = (GMimeCryptoContext *) pkcs7;
	crypto->request_passwd = request_passwd;
	
	return crypto;
#else
	return NULL;
#endif /* ENABLE_SMIME */
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
