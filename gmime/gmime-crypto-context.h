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


#ifndef __GMIME_CRYPTO_CONTEXT_H__
#define __GMIME_CRYPTO_CONTEXT_H__

#include <gmime/gmime-signature.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_CRYPTO_CONTEXT            (g_mime_crypto_context_get_type ())
#define GMIME_CRYPTO_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_CRYPTO_CONTEXT, GMimeCryptoContext))
#define GMIME_CRYPTO_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CRYPTO_CONTEXT, GMimeCryptoContextClass))
#define GMIME_IS_CRYPTO_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_CRYPTO_CONTEXT))
#define GMIME_IS_CRYPTO_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CRYPTO_CONTEXT))
#define GMIME_CRYPTO_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_CRYPTO_CONTEXT, GMimeCryptoContextClass))

#define GMIME_TYPE_DECRYPT_RESULT            (g_mime_decrypt_result_get_type ())
#define GMIME_DECRYPT_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_DECRYPT_RESULT, GMimeDecryptResult))
#define GMIME_DECRYPT_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_DECRYPT_RESULT, GMimeDecryptResultClass))
#define GMIME_IS_DECRYPT_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_DECRYPT_RESULT))
#define GMIME_IS_DECRYPT_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_DECRYPT_RESULT))
#define GMIME_DECRYPT_RESULT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_DECRYPT_RESULT, GMimeDecryptResultClass))

typedef struct _GMimeCryptoContext GMimeCryptoContext;
typedef struct _GMimeCryptoContextClass GMimeCryptoContextClass;

typedef struct _GMimeDecryptResult GMimeDecryptResult;
typedef struct _GMimeDecryptResultClass GMimeDecryptResultClass;


/**
 * GMimePasswordRequestFunc:
 * @ctx: the #GMimeCryptoContext making the request
 * @user_id: the user_id of the password being requested
 * @prompt_ctx: a string containing some helpful context for the prompt
 * @reprompt: %TRUE if this password request is a reprompt due to a previously bad password response
 * @response: a stream for the application to write the password to (followed by a newline '\n' character)
 * @err: a #GError for the callback to set if an error occurs
 *
 * A password request callback allowing a #GMimeCryptoContext to
 * prompt the user for a password for a given key.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
typedef gboolean (* GMimePasswordRequestFunc) (GMimeCryptoContext *ctx, const char *user_id, const char *prompt_ctx,
					       gboolean reprompt, GMimeStream *response, GError **err);


/**
 * GMimeCryptoContext:
 * @parent_object: parent #GObject
 * @request_passwd: a callback for requesting a password
 *
 * A crypto context for use with MIME.
 **/
struct _GMimeCryptoContext {
	GObject parent_object;
	
	GMimePasswordRequestFunc request_passwd;
};

struct _GMimeCryptoContextClass {
	GObjectClass parent_class;
	
	GMimeDigestAlgo          (* digest_id)   (GMimeCryptoContext *ctx, const char *name);
	
	const char *             (* digest_name) (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);
	
	const char *             (* get_signature_protocol) (GMimeCryptoContext *ctx);
	
	const char *             (* get_encryption_protocol) (GMimeCryptoContext *ctx);
	
	const char *             (* get_key_exchange_protocol) (GMimeCryptoContext *ctx);
	
	int                      (* sign)        (GMimeCryptoContext *ctx, const char *userid,
						  GMimeDigestAlgo digest, GMimeStream *istream,
						  GMimeStream *ostream, GError **err);
	
	GMimeSignatureList *     (* verify)      (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
						  GMimeStream *istream, GMimeStream *sigstream,
						  GError **err);
	
	int                      (* encrypt)     (GMimeCryptoContext *ctx, gboolean sign,
						  const char *userid, GMimeDigestAlgo digest,
						  GPtrArray *recipients, GMimeStream *istream,
						  GMimeStream *ostream, GError **err);
	
	GMimeDecryptResult *     (* decrypt)     (GMimeCryptoContext *ctx, GMimeStream *istream,
						  GMimeStream *ostream, GError **err);
	
	int                      (* import_keys) (GMimeCryptoContext *ctx, GMimeStream *istream,
						  GError **err);
	
	int                      (* export_keys) (GMimeCryptoContext *ctx, GPtrArray *keys,
						  GMimeStream *ostream, GError **err);
};


GType g_mime_crypto_context_get_type (void);

void g_mime_crypto_context_set_request_password (GMimeCryptoContext *ctx, GMimePasswordRequestFunc request_passwd);

/* digest algo mapping */
GMimeDigestAlgo g_mime_crypto_context_digest_id (GMimeCryptoContext *ctx, const char *name);

const char *g_mime_crypto_context_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);

/* protocol routines */
const char *g_mime_crypto_context_get_signature_protocol (GMimeCryptoContext *ctx);

const char *g_mime_crypto_context_get_encryption_protocol (GMimeCryptoContext *ctx);

const char *g_mime_crypto_context_get_key_exchange_protocol (GMimeCryptoContext *ctx);

/* crypto routines */
int g_mime_crypto_context_sign (GMimeCryptoContext *ctx, const char *userid,
				GMimeDigestAlgo digest, GMimeStream *istream,
				GMimeStream *ostream, GError **err);

GMimeSignatureList *g_mime_crypto_context_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
						  GMimeStream *istream, GMimeStream *sigstream,
						  GError **err);

int g_mime_crypto_context_encrypt (GMimeCryptoContext *ctx, gboolean sign,
				   const char *userid, GMimeDigestAlgo digest,
				   GPtrArray *recipients, GMimeStream *istream,
				   GMimeStream *ostream, GError **err);

GMimeDecryptResult *g_mime_crypto_context_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
						   GMimeStream *ostream, GError **err);

/* key/certificate routines */
int g_mime_crypto_context_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream, GError **err);

int g_mime_crypto_context_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
				       GMimeStream *ostream, GError **err);



/**
 * GMimeCipherAlgo:
 * @GMIME_CIPHER_ALGO_DEFAULT: The default (or unknown) cipher.
 * @GMIME_CIPHER_ALGO_IDEA: The IDEA cipher.
 * @GMIME_CIPHER_ALGO_3DES: The 3DES cipher.
 * @GMIME_CIPHER_ALGO_CAST5: The CAST5 cipher.
 * @GMIME_CIPHER_ALGO_BLOWFISH: The Blowfish cipher.
 * @GMIME_CIPHER_ALGO_AES: The AES (aka RIJANDALE) cipher.
 * @GMIME_CIPHER_ALGO_AES192: The AES-192 cipher.
 * @GMIME_CIPHER_ALGO_AES256: The AES-256 cipher.
 * @GMIME_CIPHER_ALGO_TWOFISH: The Twofish cipher.
 * @GMIME_CIPHER_ALGO_CAMELLIA128: The Camellia-128 cipher.
 * @GMIME_CIPHER_ALGO_CAMELLIA192: The Camellia-192 cipher.
 * @GMIME_CIPHER_ALGO_CAMELLIA256: The Camellia-256 cipher.
 *
 * A cipher algorithm.
 **/
typedef enum {
	GMIME_CIPHER_ALGO_DEFAULT     = 0,
	GMIME_CIPHER_ALGO_IDEA        = 1,
	GMIME_CIPHER_ALGO_3DES        = 2,
	GMIME_CIPHER_ALGO_CAST5       = 3,
	GMIME_CIPHER_ALGO_BLOWFISH    = 4,
	GMIME_CIPHER_ALGO_AES         = 7,
	GMIME_CIPHER_ALGO_AES192      = 8,
	GMIME_CIPHER_ALGO_AES256      = 9,
	GMIME_CIPHER_ALGO_TWOFISH     = 10,
	GMIME_CIPHER_ALGO_CAMELLIA128 = 11,
	GMIME_CIPHER_ALGO_CAMELLIA192 = 12,
	GMIME_CIPHER_ALGO_CAMELLIA256 = 13
} GMimeCipherAlgo;

/**
 * GMimeDecryptResult:
 * @parent_object: parent #GObject
 * @recipients: A #GMimeCertificateList
 * @signatures: A #GMimeSignatureList if signed or %NULL otherwise.
 * @cipher: The cipher algorithm used to encrypt the stream.
 * @mdc: The MDC digest algorithm used, if any.
 *
 * An object containing the results from decrypting an encrypted stream.
 **/
struct _GMimeDecryptResult {
	GObject parent_object;
	
	GMimeCertificateList *recipients;
	GMimeSignatureList *signatures;
	GMimeCipherAlgo cipher;
	GMimeDigestAlgo mdc;
};

struct _GMimeDecryptResultClass {
	GObjectClass parent_class;
	
};

GType g_mime_decrypt_result_get_type (void);

GMimeDecryptResult *g_mime_decrypt_result_new (void);

void g_mime_decrypt_result_set_recipients (GMimeDecryptResult *result, GMimeCertificateList *recipients);
GMimeCertificateList *g_mime_decrypt_result_get_recipients (GMimeDecryptResult *result);

void g_mime_decrypt_result_set_signatures (GMimeDecryptResult *result, GMimeSignatureList *signatures);
GMimeSignatureList *g_mime_decrypt_result_get_signatures (GMimeDecryptResult *result);

void g_mime_decrypt_result_set_cipher (GMimeDecryptResult *result, GMimeCipherAlgo cipher);
GMimeCipherAlgo g_mime_decrypt_result_get_cipher (GMimeDecryptResult *result);

void g_mime_decrypt_result_set_mdc (GMimeDecryptResult *result, GMimeDigestAlgo mdc);
GMimeDigestAlgo g_mime_decrypt_result_get_mdc (GMimeDecryptResult *result);

G_END_DECLS

#endif /* __GMIME_CRYPTO_CONTEXT_H__ */
