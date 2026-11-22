/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

/* The wire-format choices follow MimeKit's DkimPublicKeyRecord and
 * Ed25519DigestSigner (Jeffrey Stedfast); provider operations are native.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gio/gio.h>

#include "gmime-dkim-backend-private.h"

#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
/* Only accept a single unencrypted PKCS#8 or traditional RSA PEM block.
 * In particular, never ask a provider to prompt for a passphrase or try
 * the empty password when presented with encrypted key material. */
static gboolean
valid_pem (GBytes *bytes)
{
	const char *data, *begin, *end;
	gsize len;

	data = g_bytes_get_data (bytes, &len);

	while (len && g_ascii_isspace (*data)) {
		data++;
		len--;
	}

	while (len && g_ascii_isspace (data[len - 1]))
		len--;

	begin = "-----BEGIN PRIVATE KEY-----";
	end = "-----END PRIVATE KEY-----";

	if (len < strlen (begin) || memcmp (data, begin, strlen (begin)) != 0) {
		begin = "-----BEGIN RSA PRIVATE KEY-----";
		end = "-----END RSA PRIVATE KEY-----";
	}

	if (len <= strlen (begin) + strlen (end) || len > G_MAXINT ||
	    memcmp (data, begin, strlen (begin)) != 0 ||
	    memcmp (data + len - strlen (end), end, strlen (end)) != 0)
		return FALSE;

	data += strlen (begin);
	len -= strlen (begin) + strlen (end);

	while (len--) {
		char c = *data++;
		if (!g_ascii_isalnum (c) && !g_ascii_isspace (c) && c != '+' && c != '/' && c != '=')
			return FALSE;
	}

	return g_bytes_get_size (bytes) <= G_MAXINT;
}

/* Read one definite-length DER value. Check the entire outer sequence so
 * a provider accepting trailing bytes cannot silently select a prefix. */
static gboolean
read_der (const guint8 **src, gsize *left, guint8 tag, const guint8 **value, gsize *size)
{
	const guint8 *p = *src;
	gsize n = *left, len;

	if (n < 2 || *p++ != tag)
		return FALSE;

	len = *p++;
	n -= 2;

	if (len & 0x80) {
		gsize count = len & 0x7f;

		if (!count || count > sizeof (gsize) || count > n || !*p)
			return FALSE;

		n -= count;
		len = 0;

		while (count--)
			len = (len << 8) | *p++;

		if (len < 128)
			return FALSE;
	}

	if (len > n)
		return FALSE;

	*value = p;
	*size = len;
	*src = p + len;
	*left = n - len;

	return TRUE;
}

static gboolean
rsa_sequence (GBytes *key, const guint8 **seq, gsize *len)
{
	gsize left;
	const guint8 *p = g_bytes_get_data (key, &left);

	return left <= G_MAXINT && read_der (&p, &left, 0x30, seq, len) && !left && *len > 0;
}

static void
invalid_key (GError **error)
{
	/* Never include key material in diagnostics. RFC 8301 section 3.2
	 * requires rejecting RSA keys below 1024 bits, including for verification. */
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     "Invalid DKIM key (expected Ed25519 or RSA of at least 1024 bits for the selected algorithm)");
}
#endif

#if defined (HAVE_DKIM_GNUTLS)

#include <gnutls/abstract.h>
#include <gnutls/x509.h>

static gboolean
valid_key (gboolean ed25519, int type, unsigned int bits)
{
	return ed25519 ? type == GNUTLS_PK_EDDSA_ED25519 : type == GNUTLS_PK_RSA && bits >= 1024;
}

GBytes *
_g_mime_dkim_sign_digest (gboolean ed25519, GBytes *pem, const guint8 digest[32], GError **error)
{
	gnutls_datum_t in, hash = { (unsigned char *) digest, 32 }, sig = { NULL, 0 };
	gnutls_privkey_t key = NULL;
	GBytes *result = NULL;
	unsigned int bits = 0;
	int type, ret;

	if (!valid_pem (pem)) {
		invalid_key (error);
		return NULL;
	}

	in.data = (unsigned char *) g_bytes_get_data (pem, NULL);
	in.size = (unsigned int) g_bytes_get_size (pem);

	if (gnutls_privkey_init (&key) < 0)
		goto failed;

	if (gnutls_privkey_import_x509_raw (key, &in, GNUTLS_X509_FMT_PEM, NULL, 0) < 0) {
		invalid_key (error);
		goto done;
	}

	type = gnutls_privkey_get_pk_algorithm (key, &bits);

	if (!valid_key (ed25519, type, bits)) {
		invalid_key (error);
		goto done;
	}

	/* RFC 8463 section 3: the SHA-256 digest is the message to pure
	 * Ed25519, NOT Ed25519ph. RSA uses the same precomputed digest in
	 * a SHA-256 DigestInfo (RSASSA-PKCS1-v1_5), without hashing again. */
	if (ed25519)
		ret = gnutls_privkey_sign_data2 (key, GNUTLS_SIGN_EDDSA_ED25519, 0, &hash, &sig);
	else
		ret = gnutls_privkey_sign_hash2 (key, GNUTLS_SIGN_RSA_SHA256, 0, &hash, &sig);

	if (ret < 0)
		goto failed;

	result = g_bytes_new_with_free_func (sig.data, sig.size, gnutls_free, sig.data);
	goto done;

failed:
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "DKIM signing failed");

done:
	if (key)
		gnutls_privkey_deinit (key);

	return result;
}

static gboolean
import_public_key (gnutls_pubkey_t key, gboolean ed25519, GBytes *bytes)
{
	const guint8 *seq, *m, *e;
	gsize len, mlen, elen;
	gnutls_datum_t data, modulus, exponent;

	if (g_bytes_get_size (bytes) > G_MAXINT)
		return FALSE;

	data.data = (unsigned char *) g_bytes_get_data (bytes, NULL);
	data.size = (unsigned int) g_bytes_get_size (bytes);

	/* RFC 8463 section 4.2: p= is a raw 32-byte Ed25519 public key,
	 * not the DER SubjectPublicKeyInfo often exported by key tools. */
	if (ed25519)
		return data.size == 32 && gnutls_pubkey_import_ecc_raw (key, GNUTLS_ECC_CURVE_ED25519, &data, NULL) >= 0;

	/* RFC 6376 section 3.6.1 describes a bare PKCS#1 RSAPublicKey.
	 * Deployed DNS records (and RFC 8463 Appendix A) commonly wrap it
	 * in SubjectPublicKeyInfo (see erratum 3017). Neither format is an
	 * X.509 certificate. */
	if (!rsa_sequence (bytes, &seq, &len))
		return FALSE;

	if (*seq != 0x02)
		return gnutls_pubkey_import (key, &data, GNUTLS_X509_FMT_DER) >= 0;

	if (!read_der (&seq, &len, 0x02, &m, &mlen) || !mlen || (*m & 0x80) ||
	    !read_der (&seq, &len, 0x02, &e, &elen) || !elen || (*e & 0x80) || len)
		return FALSE;

	modulus.data = (unsigned char *) m;
	modulus.size = (unsigned int) mlen;
	exponent.data = (unsigned char *) e;
	exponent.size = (unsigned int) elen;

	return gnutls_pubkey_import_rsa_raw (key, &modulus, &exponent) >= 0;
}

gboolean
_g_mime_dkim_verify_digest (gboolean ed25519, GBytes *bytes, const guint8 digest[32],
			    GBytes *signature, GError **error)
{
	gnutls_datum_t hash = { (unsigned char *) digest, 32 }, sig;
	gnutls_pubkey_t key;
	unsigned int bits = 0;
	gboolean ok = FALSE;
	int type, ret;

	if (gnutls_pubkey_init (&key) < 0) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Cannot initialize DKIM verifier");
		return FALSE;
	}

	if (!import_public_key (key, ed25519, bytes)) {
		invalid_key (error);
		goto done;
	}

	type = gnutls_pubkey_get_pk_algorithm (key, &bits);

	if (!valid_key (ed25519, type, bits)) {
		invalid_key (error);
		goto done;
	}

	if (g_bytes_get_size (signature) != (ed25519 ? 64 : (bits + 7) / 8))
		goto done;

	sig.data = (unsigned char *) g_bytes_get_data (signature, NULL);
	sig.size = (unsigned int) g_bytes_get_size (signature);

	if (ed25519)
		ret = gnutls_pubkey_verify_data2 (key, GNUTLS_SIGN_EDDSA_ED25519, 0, &hash, &sig);
	else
		ret = gnutls_pubkey_verify_hash2 (key, GNUTLS_SIGN_RSA_SHA256, 0, &hash, &sig);

	ok = ret >= 0;

	if (!ok && ret != GNUTLS_E_PK_SIG_VERIFY_FAILED)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "DKIM verification failed");

done:
	gnutls_pubkey_deinit (key);

	return ok;
}

#elif defined (HAVE_DKIM_OPENSSL)

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

static int
no_password (char *buf, int size, int rwflag, void *data)
{
	return -1;
}

static gboolean
valid_key (gboolean ed25519, EVP_PKEY *key)
{
	return key && (ed25519 ? EVP_PKEY_base_id (key) == EVP_PKEY_ED25519 :
		       EVP_PKEY_base_id (key) == EVP_PKEY_RSA && EVP_PKEY_bits (key) >= 1024);
}

static EVP_PKEY_CTX *
rsa_context (EVP_PKEY *key, gboolean signing)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new (key, NULL);

	if (!ctx || (signing ? EVP_PKEY_sign_init (ctx) : EVP_PKEY_verify_init (ctx)) <= 0 ||
	    EVP_PKEY_CTX_set_rsa_padding (ctx, RSA_PKCS1_PADDING) <= 0 ||
	    EVP_PKEY_CTX_set_signature_md (ctx, EVP_sha256 ()) <= 0) {
		EVP_PKEY_CTX_free (ctx);
		return NULL;
	}

	return ctx;
}

GBytes *
_g_mime_dkim_sign_digest (gboolean ed25519, GBytes *pem, const guint8 digest[32], GError **error)
{
	EVP_PKEY_CTX *rsa = NULL;
	EVP_MD_CTX *md = NULL;
	unsigned char *sig;
	EVP_PKEY *key;
	gboolean ok;
	size_t len;
	BIO *bio;

	if (!valid_pem (pem)) {
		invalid_key (error);
		return NULL;
	}

	bio = BIO_new_mem_buf (g_bytes_get_data (pem, NULL), (int) g_bytes_get_size (pem));
	key = bio ? PEM_read_bio_PrivateKey (bio, NULL, no_password, NULL) : NULL;
	BIO_free (bio);

	if (!valid_key (ed25519, key)) {
		EVP_PKEY_free (key);
		invalid_key (error);
		return NULL;
	}

	len = EVP_PKEY_size (key);
	sig = g_malloc (len);

	/* RFC 8463 signs the 32 digest octets using one-shot pure Ed25519.
	 * For RSA use the prehashed API, not EVP_DigestSign (double hash). */
	if (ed25519) {
		md = EVP_MD_CTX_new ();
		ok = md && EVP_DigestSignInit (md, NULL, NULL, NULL, key) > 0 &&
			EVP_DigestSign (md, sig, &len, digest, 32) > 0;
	} else {
		rsa = rsa_context (key, TRUE);
		ok = rsa && EVP_PKEY_sign (rsa, sig, &len, digest, 32) > 0;
	}

	EVP_MD_CTX_free (md);
	EVP_PKEY_CTX_free (rsa);
	EVP_PKEY_free (key);

	if (ok)
		return g_bytes_new_take (sig, len);

	g_free (sig);
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "DKIM signing failed");

	return NULL;
}

gboolean
_g_mime_dkim_verify_digest (gboolean ed25519, GBytes *bytes, const guint8 digest[32],
			    GBytes *signature, GError **error)
{
	const guint8 *seq, *data, *sig;
	gsize len, size, slen;
	EVP_PKEY *key = NULL;
	int ret = -1;

	data = g_bytes_get_data (bytes, &size);

	if (ed25519) {
		if (size == 32)
			key = EVP_PKEY_new_raw_public_key (EVP_PKEY_ED25519, NULL, data, size);
	} else if (rsa_sequence (bytes, &seq, &len)) {
		/* Accept both RFC 6376's RSAPublicKey and the SubjectPublicKeyInfo
		 * used in practice (including RFC 8463 Appendix A). */
		const guint8 *p = data;

		key = *seq == 0x02 ? d2i_PublicKey (EVP_PKEY_RSA, NULL, &p, size) : d2i_PUBKEY (NULL, &p, size);

		if (p != data + size) {
			EVP_PKEY_free (key);
			key = NULL;
		}
	}

	if (!valid_key (ed25519, key)) {
		EVP_PKEY_free (key);
		invalid_key (error);
		return FALSE;
	}

	sig = g_bytes_get_data (signature, &slen);

	if (slen != (gsize) EVP_PKEY_size (key)) {
		ret = 0;
	} else if (ed25519) {
		EVP_MD_CTX *md = EVP_MD_CTX_new ();

		if (md && EVP_DigestVerifyInit (md, NULL, NULL, NULL, key) > 0)
			ret = EVP_DigestVerify (md, sig, slen, digest, 32);

		EVP_MD_CTX_free (md);
	} else {
		EVP_PKEY_CTX *rsa = rsa_context (key, FALSE);

		if (rsa)
			ret = EVP_PKEY_verify (rsa, sig, slen, digest, 32);

		EVP_PKEY_CTX_free (rsa);
	}

	EVP_PKEY_free (key);

	if (ret < 0)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "DKIM verification failed");

	return ret == 1;
}

#else

GBytes *
_g_mime_dkim_sign_digest (gboolean ed25519, GBytes *pem, const guint8 digest[32], GError **error)
{
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "DKIM crypto support is disabled");
	return NULL;
}

gboolean
_g_mime_dkim_verify_digest (gboolean ed25519, GBytes *key, const guint8 digest[32],
			    GBytes *signature, GError **error)
{
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "DKIM crypto support is disabled");
	return FALSE;
}

#endif
