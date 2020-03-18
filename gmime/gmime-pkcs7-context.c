/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
#ifdef ENABLE_CRYPTO
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-gpgme-utils.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#endif /* ENABLE_CRYPTO */
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
 * @short_description: PKCS7 crypto contexts
 * @see_also: #GMimeCryptoContext
 *
 * A #GMimePkcs7Context is a #GMimeCryptoContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/


/**
 * GMimePkcs7Context:
 *
 * A PKCS7 crypto context.
 **/
struct _GMimePkcs7Context {
	GMimeCryptoContext parent_object;
	
#ifdef ENABLE_CRYPTO
	gpgme_ctx_t ctx;
#endif
};

struct _GMimePkcs7ContextClass {
	GMimeCryptoContextClass parent_class;
	
};

static void g_mime_pkcs7_context_class_init (GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_init (GMimePkcs7Context *ctx, GMimePkcs7ContextClass *klass);
static void g_mime_pkcs7_context_finalize (GObject *object);

static GMimeDigestAlgo pkcs7_digest_id (GMimeCryptoContext *ctx, const char *name);
static const char *pkcs7_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);

static const char *pkcs7_get_signature_protocol (GMimeCryptoContext *ctx);
static const char *pkcs7_get_encryption_protocol (GMimeCryptoContext *ctx);
static const char *pkcs7_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static int pkcs7_sign (GMimeCryptoContext *ctx, gboolean detach, const char *userid,
		       GMimeStream *istream, GMimeStream *ostream, GError **err);
	
static GMimeSignatureList *pkcs7_verify (GMimeCryptoContext *ctx, GMimeVerifyFlags flags,
					 GMimeStream *istream, GMimeStream *sigstream,
					 GMimeStream *ostream, GError **err);

static int pkcs7_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid, GMimeEncryptFlags flags,
			  GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err);

static GMimeDecryptResult *pkcs7_decrypt (GMimeCryptoContext *ctx, GMimeDecryptFlags flags, const char *session_key,
					  GMimeStream *istream, GMimeStream *ostream, GError **err);

static int pkcs7_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream, GError **err);

static int pkcs7_export_keys (GMimeCryptoContext *ctx, const char *keys[],
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
g_mime_pkcs7_context_init (GMimePkcs7Context *pkcs7, GMimePkcs7ContextClass *klass)
{
#ifdef ENABLE_CRYPTO
	pkcs7->ctx = NULL;
#endif
}

static void
g_mime_pkcs7_context_finalize (GObject *object)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) object;
	
	if (pkcs7->ctx)
		gpgme_release (pkcs7->ctx);
#endif
	
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

#define set_passphrase_callback(context)

static int
pkcs7_sign (GMimeCryptoContext *context, gboolean detach, const char *userid,
	    GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	gpgme_sig_mode_t mode = detach ? GPGME_SIG_MODE_DETACH : GPGME_SIG_MODE_NORMAL;
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	set_passphrase_callback (context);
	
	return g_mime_gpgme_sign (pkcs7->ctx, mode, userid, istream, ostream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

static GMimeSignatureList *
pkcs7_verify (GMimeCryptoContext *context, GMimeVerifyFlags flags, GMimeStream *istream, GMimeStream *sigstream,
	      GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	return g_mime_gpgme_verify (pkcs7->ctx, flags, istream, sigstream, ostream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTO */
}

static int
pkcs7_encrypt (GMimeCryptoContext *context, gboolean sign, const char *userid, GMimeEncryptFlags flags,
	       GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	if (sign) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("Cannot sign and encrypt a stream at the same time using pkcs7"));
		return -1;
	}
	
	return g_mime_gpgme_encrypt (pkcs7->ctx, sign, userid, flags, recipients, istream, ostream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

static GMimeDecryptResult *
pkcs7_decrypt (GMimeCryptoContext *context, GMimeDecryptFlags flags, const char *session_key,
	       GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	set_passphrase_callback (context);
	
	return g_mime_gpgme_decrypt (pkcs7->ctx, flags, session_key, istream, ostream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTO */
}

static int
pkcs7_import_keys (GMimeCryptoContext *context, GMimeStream *istream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	set_passphrase_callback (context);
	
	return g_mime_gpgme_import (pkcs7->ctx, istream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}

static int
pkcs7_export_keys (GMimeCryptoContext *context, const char *keys[], GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTO
	GMimePkcs7Context *pkcs7 = (GMimePkcs7Context *) context;
	
	set_passphrase_callback (context);
	
	return g_mime_gpgme_export (pkcs7->ctx, keys, ostream, err);
#else
	g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("S/MIME support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTO */
}


/**
 * g_mime_pkcs7_context_new:
 *
 * Creates a new pkcs7 crypto context object.
 *
 * Returns: (transfer full): a new pkcs7 crypto context object.
 **/
GMimeCryptoContext *
g_mime_pkcs7_context_new (void)
{
#ifdef ENABLE_CRYPTO
	gpgme_keylist_mode_t keylist_mode;
	GMimePkcs7Context *pkcs7;
	gpgme_ctx_t ctx;
	
	/* make sure GpgMe supports the CMS protocols */
	if (gpgme_engine_check_version (GPGME_PROTOCOL_CMS) != GPG_ERR_NO_ERROR)
		return NULL;
	
	/* create the GpgMe context */
	if (gpgme_new (&ctx) != GPG_ERR_NO_ERROR)
		return NULL;
	
	pkcs7 = g_object_new (GMIME_TYPE_PKCS7_CONTEXT, NULL);
	gpgme_set_protocol (ctx, GPGME_PROTOCOL_CMS);
	gpgme_set_textmode (ctx, FALSE);
	gpgme_set_armor (ctx, FALSE);
	
	/* ensure that key listings are correctly validated, since we
	   use user ID validity to determine what identity to report */
	keylist_mode = gpgme_get_keylist_mode (ctx);
	if (!(keylist_mode & GPGME_KEYLIST_MODE_VALIDATE)) {
		if (gpgme_set_keylist_mode (ctx, keylist_mode | GPGME_KEYLIST_MODE_VALIDATE) != GPG_ERR_NO_ERROR) {
			gpgme_release (ctx);
			return NULL;
		}
	}
	
	pkcs7->ctx = ctx;
	
	return (GMimeCryptoContext *) pkcs7;
#else
	return NULL;
#endif /* ENABLE_CRYPTO */
}
