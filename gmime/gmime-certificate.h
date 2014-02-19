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


#ifndef __GMIME_CERTIFICATE_H__
#define __GMIME_CERTIFICATE_H__

#include <glib.h>
#include <glib-object.h>

#include <time.h>

G_BEGIN_DECLS

#define GMIME_TYPE_CERTIFICATE                  (g_mime_certificate_get_type ())
#define GMIME_CERTIFICATE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_CERTIFICATE, GMimeCertificate))
#define GMIME_CERTIFICATE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CERTIFICATE, GMimeCertificateClass))
#define GMIME_IS_CERTIFICATE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_CERTIFICATE))
#define GMIME_IS_CERTIFICATE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CERTIFICATE))
#define GMIME_CERTIFICATE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_CERTIFICATE, GMimeCertificateClass))

#define GMIME_TYPE_CERTIFICATE_LIST             (g_mime_certificate_list_get_type ())
#define GMIME_CERTIFICATE_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_CERTIFICATE_LIST, GMimeCertificateList))
#define GMIME_CERTIFICATE_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CERTIFICATE_LIST, GMimeCertificateListClass))
#define GMIME_IS_CERTIFICATE_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_CERTIFICATE_LIST))
#define GMIME_IS_CERTIFICATE_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CERTIFICATE_LIST))
#define GMIME_CERTIFICATE_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_CERTIFICATE_LIST, GMimeCertificateListClass))


typedef struct _GMimeCertificate GMimeCertificate;
typedef struct _GMimeCertificateClass GMimeCertificateClass;

typedef struct _GMimeCertificateList GMimeCertificateList;
typedef struct _GMimeCertificateListClass GMimeCertificateListClass;


/**
 * GMimeDigestAlgo:
 * @GMIME_DIGEST_ALGO_DEFAULT: The default hash algorithm.
 * @GMIME_DIGEST_ALGO_MD5: The MD5 hash algorithm.
 * @GMIME_DIGEST_ALGO_SHA1: The SHA-1 hash algorithm.
 * @GMIME_DIGEST_ALGO_RIPEMD160: The RIPEMD-160 hash algorithm.
 * @GMIME_DIGEST_ALGO_MD2: The MD2 hash algorithm.
 * @GMIME_DIGEST_ALGO_TIGER192: The TIGER-192 hash algorithm.
 * @GMIME_DIGEST_ALGO_HAVAL5160: The HAVAL-5-160 hash algorithm.
 * @GMIME_DIGEST_ALGO_SHA256: The SHA-256 hash algorithm.
 * @GMIME_DIGEST_ALGO_SHA384: The SHA-384 hash algorithm.
 * @GMIME_DIGEST_ALGO_SHA512: The SHA-512 hash algorithm.
 * @GMIME_DIGEST_ALGO_SHA224: The SHA-224 hash algorithm.
 * @GMIME_DIGEST_ALGO_MD4: The MD4 hash algorithm.
 *
 * A hash algorithm.
 **/
typedef enum {
	GMIME_DIGEST_ALGO_DEFAULT     = 0,
	GMIME_DIGEST_ALGO_MD5         = 1,
	GMIME_DIGEST_ALGO_SHA1        = 2,
	GMIME_DIGEST_ALGO_RIPEMD160   = 3,
	GMIME_DIGEST_ALGO_MD2         = 5,
	GMIME_DIGEST_ALGO_TIGER192    = 6,
	GMIME_DIGEST_ALGO_HAVAL5160   = 7,
	GMIME_DIGEST_ALGO_SHA256      = 8,
	GMIME_DIGEST_ALGO_SHA384      = 9,
	GMIME_DIGEST_ALGO_SHA512      = 10,
	GMIME_DIGEST_ALGO_SHA224      = 11,
	GMIME_DIGEST_ALGO_MD4         = 301
} GMimeDigestAlgo;

/**
 * GMimePubKeyAlgo:
 * @GMIME_PUBKEY_ALGO_DEFAULT: The default public-key algorithm.
 * @GMIME_PUBKEY_ALGO_RSA: The RSA algorithm.
 * @GMIME_PUBKEY_ALGO_RSA_E: An encryption-only RSA algorithm.
 * @GMIME_PUBKEY_ALGO_RSA_S: A signature-only RSA algorithm.
 * @GMIME_PUBKEY_ALGO_ELG_E: An encryption-only ElGamal algorithm.
 * @GMIME_PUBKEY_ALGO_DSA: The DSA algorithm.
 * @GMIME_PUBKEY_ALGO_ELG: The ElGamal algorithm.
 *
 * A public-key algorithm.
 **/
typedef enum {
	GMIME_PUBKEY_ALGO_DEFAULT  = 0,
	GMIME_PUBKEY_ALGO_RSA      = 1,
	GMIME_PUBKEY_ALGO_RSA_E    = 2,
	GMIME_PUBKEY_ALGO_RSA_S    = 3,
	GMIME_PUBKEY_ALGO_ELG_E    = 16,
	GMIME_PUBKEY_ALGO_DSA      = 17,
	GMIME_PUBKEY_ALGO_ELG      = 20
} GMimePubKeyAlgo;

/**
 * GMimeCertificateTrust:
 * @GMIME_CERTIFICATE_TRUST_NONE: No trust assigned.
 * @GMIME_CERTIFICATE_TRUST_NEVER: Never trust this certificate.
 * @GMIME_CERTIFICATE_TRUST_UNDEFINED: Undefined trust for this certificate.
 * @GMIME_CERTIFICATE_TRUST_MARGINAL: Trust this certificate maginally.
 * @GMIME_CERTIFICATE_TRUST_FULLY: Trust this certificate fully.
 * @GMIME_CERTIFICATE_TRUST_ULTIMATE: Trust this certificate ultimately.
 *
 * The trust value of a certificate.
 **/
typedef enum {
	GMIME_CERTIFICATE_TRUST_NONE,
	GMIME_CERTIFICATE_TRUST_NEVER,
	GMIME_CERTIFICATE_TRUST_UNDEFINED,
	GMIME_CERTIFICATE_TRUST_MARGINAL,
	GMIME_CERTIFICATE_TRUST_FULLY,
	GMIME_CERTIFICATE_TRUST_ULTIMATE
} GMimeCertificateTrust;

/**
 * GMimeCertificate:
 * @parent_object: parent #GObject
 * @pubkey_algo: The public-key algorithm used by the certificate, if known.
 * @digest_algo: The digest algorithm used by the certificate, if known.
 * @trust: The level of trust assigned to this certificate.
 * @issuer_serial: The issuer of the certificate, if known.
 * @issuer_name: The issuer of the certificate, if known.
 * @fingerprint: A hex string representing the certificate's fingerprint.
 * @created: The creation date of the certificate.
 * @expires: The expiration date of the certificate.
 * @keyid: The certificate's key id.
 * @email: The email address of the person or entity.
 * @name: The name of the person or entity.
 *
 * An object containing useful information about a certificate.
 **/
struct _GMimeCertificate {
	GObject parent_object;
	
	GMimePubKeyAlgo pubkey_algo;
	GMimeDigestAlgo digest_algo;
	GMimeCertificateTrust trust;
	char *issuer_serial;
	char *issuer_name;
	char *fingerprint;
	time_t created;
	time_t expires;
	char *keyid;
	char *email;
	char *name;
};

struct _GMimeCertificateClass {
	GObjectClass parent_class;
	
};


GType g_mime_certificate_get_type (void);

GMimeCertificate *g_mime_certificate_new (void);

void g_mime_certificate_set_trust (GMimeCertificate *cert, GMimeCertificateTrust trust);
GMimeCertificateTrust g_mime_certificate_get_trust (GMimeCertificate *cert);

void g_mime_certificate_set_pubkey_algo (GMimeCertificate *cert, GMimePubKeyAlgo algo);
GMimePubKeyAlgo g_mime_certificate_get_pubkey_algo (GMimeCertificate *cert);

void g_mime_certificate_set_digest_algo (GMimeCertificate *cert, GMimeDigestAlgo algo);
GMimeDigestAlgo g_mime_certificate_get_digest_algo (GMimeCertificate *cert);

void g_mime_certificate_set_issuer_serial (GMimeCertificate *cert, const char *issuer_serial);
const char *g_mime_certificate_get_issuer_serial (GMimeCertificate *cert);

void g_mime_certificate_set_issuer_name (GMimeCertificate *cert, const char *issuer_name);
const char *g_mime_certificate_get_issuer_name (GMimeCertificate *cert);

void g_mime_certificate_set_fingerprint (GMimeCertificate *cert, const char *fingerprint);
const char *g_mime_certificate_get_fingerprint (GMimeCertificate *cert);

void g_mime_certificate_set_key_id (GMimeCertificate *cert, const char *key_id);
const char *g_mime_certificate_get_key_id (GMimeCertificate *cert);

void g_mime_certificate_set_email (GMimeCertificate *cert, const char *email);
const char *g_mime_certificate_get_email (GMimeCertificate *cert);

void g_mime_certificate_set_name (GMimeCertificate *cert, const char *name);
const char *g_mime_certificate_get_name (GMimeCertificate *cert);

void g_mime_certificate_set_created (GMimeCertificate *cert, time_t created);
time_t g_mime_certificate_get_created (GMimeCertificate *cert);

void g_mime_certificate_set_expires (GMimeCertificate *cert, time_t expires);
time_t g_mime_certificate_get_expires (GMimeCertificate *cert);


/**
 * GMimeCertificateList:
 * @parent_object: parent #GObject
 * @array: An array of #GMimeCertificate objects.
 *
 * A collection of #GMimeCertificate objects.
 **/
struct _GMimeCertificateList {
	GObject parent_object;
	GPtrArray *array;
};

struct _GMimeCertificateListClass {
	GObjectClass parent_class;
	
};


GType g_mime_certificate_list_get_type (void);

GMimeCertificateList *g_mime_certificate_list_new (void);

int g_mime_certificate_list_length (GMimeCertificateList *list);

void g_mime_certificate_list_clear (GMimeCertificateList *list);

int g_mime_certificate_list_add (GMimeCertificateList *list, GMimeCertificate *cert);
void g_mime_certificate_list_insert (GMimeCertificateList *list, int index, GMimeCertificate *cert);
gboolean g_mime_certificate_list_remove (GMimeCertificateList *list, GMimeCertificate *cert);
gboolean g_mime_certificate_list_remove_at (GMimeCertificateList *list, int index);

gboolean g_mime_certificate_list_contains (GMimeCertificateList *list, GMimeCertificate *cert);
int g_mime_certificate_list_index_of (GMimeCertificateList *list, GMimeCertificate *cert);

GMimeCertificate *g_mime_certificate_list_get_certificate (GMimeCertificateList *list, int index);
void g_mime_certificate_list_set_certificate (GMimeCertificateList *list, int index, GMimeCertificate *cert);

G_END_DECLS

#endif /* __GMIME_CERTIFICATE_H__ */
