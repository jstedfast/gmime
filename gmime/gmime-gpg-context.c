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

#include "gmime-gpg-context.h"
#ifdef ENABLE_CRYPTO
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-pipe.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#endif /* ENABLE_CRYPTO */
#include "gmime-error.h"

#include <gpgme.h>

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


/**
 * SECTION: gmime-gpg-context
 * @title: GMimeGpgContext
 * @short_description: GnuPG crypto contexts
 * @see_also: #GMimeCryptoContext
 *
 * A #GMimeGpgContext is a #GMimeCryptoContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/


/**
 * GMimeGpgContext:
 *
 * A GnuPG crypto context.
 **/
struct _GMimeGpgContext {
	GMimeCryptoContext parent_object;
	
#ifdef ENABLE_CRYPTO
	gpgme_encrypt_flags_t encrypt_flags;
	gboolean retrieve_session_key;
	gboolean auto_key_retrieve;
	gpgme_ctx_t ctx;
#endif
};

struct _GMimeGpgContextClass {
	GMimeCryptoContextClass parent_class;
	
};


static void g_mime_gpg_context_class_init (GMimeGpgContextClass *klass);
static void g_mime_gpg_context_init (GMimeGpgContext *ctx, GMimeGpgContextClass *klass);
static void g_mime_gpg_context_finalize (GObject *object);

static GMimeDigestAlgo gpg_digest_id (GMimeCryptoContext *ctx, const char *name);
static const char *gpg_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);

static gboolean gpg_get_retrieve_session_key (GMimeCryptoContext *context);
static int gpg_set_retrieve_session_key (GMimeCryptoContext *ctx, gboolean retrieve_session_key,
					 GError **err);

static gboolean gpg_get_always_trust (GMimeCryptoContext *context);
static void gpg_set_always_trust (GMimeCryptoContext *ctx, gboolean always_trust);

static int gpg_sign (GMimeCryptoContext *ctx, const char *userid,
		     GMimeDigestAlgo digest, GMimeStream *istream,
		     GMimeStream *ostream, GError **err);

static const char *gpg_get_signature_protocol (GMimeCryptoContext *ctx);
static const char *gpg_get_encryption_protocol (GMimeCryptoContext *ctx);
static const char *gpg_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static GMimeSignatureList *gpg_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
				       GMimeStream *istream, GMimeStream *sigstream,
				       GError **err);

static int gpg_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid,
			GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
			GMimeStream *ostream, GError **err);

static GMimeDecryptResult *gpg_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
					GMimeStream *ostream, GError **err);

static GMimeDecryptResult *gpg_decrypt_session (GMimeCryptoContext *ctx, const char *session_key,
						GMimeStream *istream, GMimeStream *ostream,
						GError **err);

static int gpg_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream,
			    GError **err);

static int gpg_export_keys (GMimeCryptoContext *ctx, const char *keys[],
			    GMimeStream *ostream, GError **err);


static GMimeCryptoContextClass *parent_class = NULL;


GType
g_mime_gpg_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeGpgContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_gpg_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeGpgContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_gpg_context_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_CRYPTO_CONTEXT, "GMimeGpgContext", &info, 0);
	}
	
	return type;
}


static void
g_mime_gpg_context_class_init (GMimeGpgContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeCryptoContextClass *crypto_class = GMIME_CRYPTO_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_gpg_context_finalize;
	
	crypto_class->digest_id = gpg_digest_id;
	crypto_class->digest_name = gpg_digest_name;
	crypto_class->sign = gpg_sign;
	crypto_class->verify = gpg_verify;
	crypto_class->encrypt = gpg_encrypt;
	crypto_class->decrypt = gpg_decrypt;
	crypto_class->decrypt_session = gpg_decrypt_session;
	crypto_class->import_keys = gpg_import_keys;
	crypto_class->export_keys = gpg_export_keys;
	crypto_class->get_signature_protocol = gpg_get_signature_protocol;
	crypto_class->get_encryption_protocol = gpg_get_encryption_protocol;
	crypto_class->get_key_exchange_protocol = gpg_get_key_exchange_protocol;
	crypto_class->get_retrieve_session_key = gpg_get_retrieve_session_key;
	crypto_class->set_retrieve_session_key = gpg_set_retrieve_session_key;
	crypto_class->get_always_trust = gpg_get_always_trust;
	crypto_class->set_always_trust = gpg_set_always_trust;
}

static void
g_mime_gpg_context_init (GMimeGpgContext *gpg, GMimeGpgContextClass *klass)
{
#ifdef ENABLE_CRYPTO
	gpg->retrieve_session_key = FALSE;
	gpg->auto_key_retrieve = FALSE;
	gpg->encrypt_flags = 0;
	gpg->ctx = NULL;
#endif
}

static void
g_mime_gpg_context_finalize (GObject *object)
{
	GMimeGpgContext *gpg = (GMimeGpgContext *) object;
	
#ifdef ENABLE_CRYPTO
	if (gpg->ctx)
		gpgme_release (gpg->ctx);
#endif
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GMimeDigestAlgo
gpg_digest_id (GMimeCryptoContext *ctx, const char *name)
{
	if (name == NULL)
		return GMIME_DIGEST_ALGO_DEFAULT;
	
	if (!g_ascii_strcasecmp (name, "pgp-"))
		name += 4;
	
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
gpg_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest)
{
	switch (digest) {
	case GMIME_DIGEST_ALGO_MD2:
		return "pgp-md2";
	case GMIME_DIGEST_ALGO_MD4:
		return "pgp-md4";
	case GMIME_DIGEST_ALGO_MD5:
		return "pgp-md5";
	case GMIME_DIGEST_ALGO_SHA1:
		return "pgp-sha1";
	case GMIME_DIGEST_ALGO_SHA224:
		return "pgp-sha224";
	case GMIME_DIGEST_ALGO_SHA256:
		return "pgp-sha256";
	case GMIME_DIGEST_ALGO_SHA384:
		return "pgp-sha384";
	case GMIME_DIGEST_ALGO_SHA512:
		return "pgp-sha512";
	case GMIME_DIGEST_ALGO_RIPEMD160:
		return "pgp-ripemd160";
	case GMIME_DIGEST_ALGO_TIGER192:
		return "pgp-tiger192";
	case GMIME_DIGEST_ALGO_HAVAL5160:
		return "pgp-haval-5-160";
	default:
		return "pgp-sha1";
	}
}

static const char *
gpg_get_signature_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-signature";
}

static const char *
gpg_get_encryption_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-encrypted";
}

static const char *
gpg_get_key_exchange_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-keys";
}

#ifdef ENABLE_CRYPTO
static gpgme_error_t
gpg_passphrase_cb (void *hook, const char *uid_hint, const char *passphrase_info, int prev_was_bad, int fd)
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
gpg_stream_read (void *stream, void *buffer, size_t size)
{
	return g_mime_stream_read ((GMimeStream *) stream, (char *) buffer, size);
}

static ssize_t
gpg_stream_write (void *stream, const void *buffer, size_t size)
{
	return g_mime_stream_write ((GMimeStream *) stream, (const char *) buffer, size);
}

static off_t
gpg_stream_seek (void *stream, off_t offset, int whence)
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
gpg_stream_free (void *stream)
{
	/* no-op */
}

static struct gpgme_data_cbs gpg_stream_funcs = {
	gpg_stream_read,
	gpg_stream_write,
	gpg_stream_seek,
	gpg_stream_free
};



#define KEY_IS_OK(k)   (!((k)->expired || (k)->revoked ||	\
                          (k)->disabled || (k)->invalid))

static gpgme_key_t
gpg_get_key_by_name (GMimeGpgContext *gpg, const char *name, gboolean secret, GError **err)
{
	time_t now = time (NULL);
	gpgme_key_t key = NULL;
	gpgme_subkey_t subkey;
	gboolean bad = FALSE;
	gpgme_error_t error;
	int errval = 0;
	
	if ((error = gpgme_op_keylist_start (gpg->ctx, name, secret)) != GPG_ERR_NO_ERROR) {
		if (secret)
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list secret keys for \"%s\""), name);
		else
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not list keys for \"%s\""), name);
		return NULL;
	}
	
	while ((error = gpgme_op_keylist_next (gpg->ctx, &key)) == GPG_ERR_NO_ERROR) {
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
	
	gpgme_op_keylist_end (gpg->ctx);
	
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
gpg_add_signer (GMimeGpgContext *gpg, const char *signer, GError **err)
{
	gpgme_key_t key = NULL;
	
	if (!(key = gpg_get_key_by_name (gpg, signer, TRUE, err)))
		return FALSE;
	
	/* set the key (the previous operation guaranteed that it exists, no need
	 * 2 check return values...) */
	gpgme_signers_add (gpg->ctx, key);
	gpgme_key_unref (key);
	
	return TRUE;
}
#endif /* ENABLE_CRYPTO */

static int
gpg_sign (GMimeCryptoContext *context, const char *userid, GMimeDigestAlgo digest,
	  GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	gpgme_sign_result_t result;
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	if (!gpg_add_signer (gpg, userid, err))
		return -1;
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		gpgme_signers_clear (gpg->ctx);
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_signers_clear (gpg->ctx);
		gpgme_data_release (input);
		return -1;
	}
	
	/* sign the input stream */
	if ((error = gpgme_op_sign (gpg->ctx, input, output, GPGME_SIG_MODE_DETACH)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Signing failed"));
		gpgme_signers_clear (gpg->ctx);
		gpgme_data_release (output);
		gpgme_data_release (input);
		return -1;
	}
	
	gpgme_signers_clear (gpg->ctx);
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	/* return the digest algorithm used for signing */
	result = gpgme_op_sign_result (gpg->ctx);
	
	return gpg_digest_id (context, gpgme_hash_algo_name (result->signatures->hash_algo));
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

#ifdef ENABLE_CRYPTO
static GMimeCertificateTrust
gpg_trust (gpgme_validity_t trust)
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
gpg_get_signatures (GMimeGpgContext *gpg, gboolean verify)
{
	GMimeSignatureList *signatures;
	GMimeSignature *signature;
	gpgme_verify_result_t result;
	gpgme_signature_t sig;
	gpgme_subkey_t subkey;
	gpgme_user_id_t uid;
	gpgme_key_t key;
	
	/* get the signature verification results from GpgMe */
	if (!(result = gpgme_op_verify_result (gpg->ctx)) || !result->signatures)
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
		
		if (gpgme_get_key (gpg->ctx, sig->fpr, &key, 0) == GPG_ERR_NO_ERROR && key) {
			/* get more signer info from their signing key */
			g_mime_certificate_set_trust (signature->cert, gpg_trust (key->owner_trust));
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
#endif /* ENABLE_CRYPTO */

static GMimeSignatureList *
gpg_verify (GMimeCryptoContext *context, GMimeDigestAlgo digest,
	    GMimeStream *istream, GMimeStream *sigstream,
	    GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	gpgme_data_t message, signature;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&message, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return NULL;
	}
	
	/* if @sigstream is non-NULL, then it is a detached signature */
	if (sigstream != NULL) {
		if ((error = gpgme_data_new_from_cbs (&signature, &gpg_stream_funcs, sigstream)) != GPG_ERR_NO_ERROR) {
			g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open signature stream"));
			gpgme_data_release (message);
			return NULL;
		}
	} else {
		signature = NULL;
	}
	
	if ((error = gpgme_op_verify (gpg->ctx, signature, message, NULL)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not verify gpg signature"));
		if (signature)
			gpgme_data_release (signature);
		gpgme_data_release (message);
		return NULL;
	}
	
	if (signature)
		gpgme_data_release (signature);
	
	if (message)
		gpgme_data_release (message);
	
	/* get/return the gpg signatures */
	return gpg_get_signatures (gpg, TRUE);
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTO */
}

#ifdef ENABLE_CRYPTO
static void
key_list_free (gpgme_key_t *keys)
{
	gpgme_key_t *key = keys;
	
	while (*key != NULL) {
		gpgme_key_unref (*key);
		key++;
	}
	
	g_free (keys);
}
#endif /* ENABLE_CRYPTO */

static int
gpg_encrypt (GMimeCryptoContext *context, gboolean sign, const char *userid,
	     GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
	     GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	gpgme_data_t input, output;
	gpgme_error_t error;
	gpgme_key_t *rcpts;
	gpgme_key_t key;
	guint i;
	
	/* create an array of recipient keys for GpgMe */
	rcpts = g_new0 (gpgme_key_t, recipients->len + 1);
	for (i = 0; i < recipients->len; i++) {
		if (!(key = gpg_get_key_by_name (gpg, recipients->pdata[i], FALSE, err))) {
			key_list_free (rcpts);
			return -1;
		}
		
		rcpts[i] = key;
	}
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		key_list_free (rcpts);
		return -1;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_data_release (input);
		key_list_free (rcpts);
		return -1;
	}
	
	/* encrypt the input stream */
	if (sign) {
		if (!gpg_add_signer (gpg, userid, err)) {
			gpgme_data_release (output);
			gpgme_data_release (input);
			key_list_free (rcpts);
			return -1;
		}
		
		error = gpgme_op_encrypt_sign (gpg->ctx, rcpts, gpg->encrypt_flags, input, output);
		
		gpgme_signers_clear (gpg->ctx);
	} else {
		error = gpgme_op_encrypt (gpg->ctx, rcpts, gpg->encrypt_flags, input, output);
	}
	
	gpgme_data_release (output);
	gpgme_data_release (input);
	key_list_free (rcpts);
	
	if (error != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Encryption failed"));
		return -1;
	}
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

#ifdef ENABLE_CRYPTO
static GMimeDecryptResult *
gpg_get_decrypt_result (GMimeGpgContext *gpg)
{
	GMimeDecryptResult *result;
	gpgme_decrypt_result_t res;
	gpgme_recipient_t recipient;
	GMimeCertificate *cert;
	
	result = g_mime_decrypt_result_new ();
	result->recipients = g_mime_certificate_list_new ();
	result->signatures = gpg_get_signatures (gpg, FALSE);
	
	// TODO: ciper, mdc
	
	if (!(res = gpgme_op_decrypt_result (gpg->ctx)) || !res->recipients)
		return result;
	
	//if (res->session_key)
	//	result->session_key = g_strdup (res->session_key);
	
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

static GMimeDecryptResult *
gpg_decrypt (GMimeCryptoContext *context, GMimeStream *istream,
	     GMimeStream *ostream, GError **err)
{
	return gpg_decrypt_session (context, NULL, istream, ostream, err);
}

static GMimeDecryptResult *
gpg_decrypt_session (GMimeCryptoContext *context, const char *session_key,
		     GMimeStream *istream, GMimeStream *ostream,
		     GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	GMimeDecryptResult *result;
	gpgme_decrypt_result_t res;
	gpgme_data_t input, output;
	gpgme_error_t error;
	
	// TODO: make use of the session_key
	
	if ((error = gpgme_data_new_from_cbs (&input, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return NULL;
	}
	
	if ((error = gpgme_data_new_from_cbs (&output, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		gpgme_data_release (input);
		return NULL;
	}
	
	/* decrypt the input stream */
	if ((error = gpgme_op_decrypt_verify (gpg->ctx, input, output)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Decryption failed"));
		gpgme_data_release (output);
		gpgme_data_release (input);
		return NULL;
	}
	
	gpgme_data_release (output);
	gpgme_data_release (input);
	
	return gpg_get_decrypt_result (gpg);
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTO */
}

static int
gpg_import_keys (GMimeCryptoContext *context, GMimeStream *istream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	gpgme_data_t keydata;
	gpgme_error_t error;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &gpg_stream_funcs, istream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open input stream"));
		return -1;
	}
	
	/* import the key(s) */
	if ((error = gpgme_op_import (gpg->ctx, keydata)) != GPG_ERR_NO_ERROR) {
		//printf ("import error (%d): %s\n", error & GPG_ERR_CODE_MASK, gpg_strerror (error));
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not import key data"));
		gpgme_data_release (keydata);
		return -1;
	}
	
	gpgme_data_release (keydata);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

static int
gpg_export_keys (GMimeCryptoContext *context, const char *keys[], GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	gpgme_data_t keydata;
	gpgme_error_t error;
	guint i;
	
	if ((error = gpgme_data_new_from_cbs (&keydata, &gpg_stream_funcs, ostream)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not open output stream"));
		return -1;
	}
	
	/* export the key(s) */
	if ((error = gpgme_op_export_ext (gpg->ctx, keys, 0, keydata)) != GPG_ERR_NO_ERROR) {
		g_set_error (err, GMIME_GPGME_ERROR, error, _("Could not export key data"));
		gpgme_data_release (keydata);
		return -1;
	}
	
	gpgme_data_release (keydata);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

static gboolean
gpg_get_retrieve_session_key (GMimeCryptoContext *context)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	
	//return ctx->retrieve_session_key;
	return FALSE;
}


static int
gpg_set_retrieve_session_key (GMimeCryptoContext *context, gboolean retrieve_session_key, GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	
	//ctx->retrieve_session_key = retrieve_session_key;
	
	return 0;
}

static gboolean
gpg_get_always_trust (GMimeCryptoContext *context)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	
	return (gpg->encrypt_flags & GPGME_ENCRYPT_ALWAYS_TRUST) != 0;
#else
	return FALSE;
#endif /* ENABLE_CRYPTO */
}

static void
gpg_set_always_trust (GMimeCryptoContext *context, gboolean always_trust)
{
#ifdef ENABLE_CRYPTO
	GMimeGpgContext *gpg = (GMimeGpgContext *) context;
	
	if (always_trust)
		gpg->encrypt_flags |= GPGME_ENCRYPT_ALWAYS_TRUST;
	else
		gpg->encrypt_flags &= ~GPGME_ENCRYPT_ALWAYS_TRUST;
#endif /* ENABLE_CRYPTO */
}


/**
 * g_mime_gpg_context_new:
 * @request_passwd: a #GMimePasswordRequestFunc
 *
 * Creates a new gpg crypto context object.
 *
 * Returns: (transfer full): a new gpg crypto context object.
 **/
GMimeCryptoContext *
g_mime_gpg_context_new (GMimePasswordRequestFunc request_passwd)
{
#ifdef ENABLE_CRYPTO
	GMimeCryptoContext *crypto;
	GMimeGpgContext *gpg;
	gpgme_ctx_t ctx;
	
	/* make sure GpgMe supports the OpenPGP protocols */
	if (gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP) != GPG_ERR_NO_ERROR)
		return NULL;
	
	/* create the GpgMe context */
	if (gpgme_new (&ctx) != GPG_ERR_NO_ERROR)
		return NULL;
	
	gpg = g_object_newv (GMIME_TYPE_GPG_CONTEXT, 0, NULL);
	gpgme_set_passphrase_cb (ctx, gpg_passphrase_cb, gpg);
	gpgme_set_protocol (ctx, GPGME_PROTOCOL_OpenPGP);
	gpgme_set_armor (ctx, TRUE);
	gpg->ctx = ctx;
	
	crypto = (GMimeCryptoContext *) gpg;
	crypto->request_passwd = request_passwd;
	
	return crypto;
#else
	return NULL;
#endif /* ENABLE_CRYPTO */
}


/**
 * g_mime_gpg_context_get_auto_key_retrieve:
 * @ctx: a #GMimeGpgContext
 *
 * Gets whether or not gpg should auto-retrieve keys from a keyserver
 * when verifying signatures.
 *
 * Returns: %TRUE if gpg should auto-retrieve keys from a keyserver or
 * %FALSE otherwise.
 **/
gboolean
g_mime_gpg_context_get_auto_key_retrieve (GMimeGpgContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_GPG_CONTEXT (ctx), FALSE);
	
	return ctx->auto_key_retrieve;
}


/**
 * g_mime_gpg_context_set_auto_key_retrieve:
 * @ctx: a #GMimeGpgContext
 * @auto_key_retrieve: %TRUE if gpg should auto-retrieve keys from a keys server
 *
 * Sets whether or not gpg should auto-retrieve keys from a keyserver
 * when verifying signatures.
 **/
void
g_mime_gpg_context_set_auto_key_retrieve (GMimeGpgContext *ctx, gboolean auto_key_retrieve)
{
	g_return_if_fail (GMIME_IS_GPG_CONTEXT (ctx));
	
	ctx->auto_key_retrieve = auto_key_retrieve;
}
