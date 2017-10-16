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
 * @GMIME_DIGEST_ALGO_CRC32: The CRC32 hash algorithm.
 * @GMIME_DIGEST_ALGO_CRC32_RFC1510: The rfc1510 CRC32 hash algorithm.
 * @GMIME_DIGEST_ALGO_CRC32_RFC2440: The rfc2440 CRC32 hash algorithm.
 *
 * A hash algorithm.
 **/
typedef enum {
	GMIME_DIGEST_ALGO_DEFAULT       = 0,
	GMIME_DIGEST_ALGO_MD5           = 1,
	GMIME_DIGEST_ALGO_SHA1          = 2,
	GMIME_DIGEST_ALGO_RIPEMD160     = 3,
	GMIME_DIGEST_ALGO_MD2           = 5,
	GMIME_DIGEST_ALGO_TIGER192      = 6,
	GMIME_DIGEST_ALGO_HAVAL5160     = 7,
	GMIME_DIGEST_ALGO_SHA256        = 8,
	GMIME_DIGEST_ALGO_SHA384        = 9,
	GMIME_DIGEST_ALGO_SHA512        = 10,
	GMIME_DIGEST_ALGO_SHA224        = 11,
	GMIME_DIGEST_ALGO_MD4           = 301,
	GMIME_DIGEST_ALGO_CRC32         = 302,
	GMIME_DIGEST_ALGO_CRC32_RFC1510 = 303,
	GMIME_DIGEST_ALGO_CRC32_RFC2440 = 304
} GMimeDigestAlgo;

/**
 * GMimePubKeyAlgo:
 * @GMIME_PUBKEY_ALGO_DEFAULT: The default public-key algorithm.
 * @GMIME_PUBKEY_ALGO_RSA: The RSA algorithm.
 * @GMIME_PUBKEY_ALGO_RSA_E: An encryption-only RSA algorithm.
 * @GMIME_PUBKEY_ALGO_RSA_S: A signature-only RSA algorithm.
 * @GMIME_PUBKEY_ALGO_ELG_E: An encryption-only ElGamal algorithm.
 * @GMIME_PUBKEY_ALGO_DSA: The DSA algorithm.
 * @GMIME_PUBKEY_ALGO_ECC: The Eliptic Curve algorithm.
 * @GMIME_PUBKEY_ALGO_ELG: The ElGamal algorithm.
 * @GMIME_PUBKEY_ALGO_ECDSA: The Eliptic Curve + DSA algorithm.
 * @GMIME_PUBKEY_ALGO_ECDH: The Eliptic Curve + Diffie Helman algorithm.
 * @GMIME_PUBKEY_ALGO_EDDSA: The Eliptic Curve + DSA algorithm.
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
	GMIME_PUBKEY_ALGO_ECC      = 18,
	GMIME_PUBKEY_ALGO_ELG      = 20,
	GMIME_PUBKEY_ALGO_ECDSA    = 301,
	GMIME_PUBKEY_ALGO_ECDH     = 302,
	GMIME_PUBKEY_ALGO_EDDSA    = 303
} GMimePubKeyAlgo;

/**
 * GMimeTrust:
 * @GMIME_TRUST_UNKNOWN: We do not know whether to rely on identity assertions made by the certificate.
 * @GMIME_TRUST_UNDEFINED: We do not have enough information to decide whether to rely on identity assertions made by the certificate.
 * @GMIME_TRUST_NEVER: We should never rely on identity assertions made by the certificate.
 * @GMIME_TRUST_MARGINAL: We can rely on identity assertions made by this certificate as long as they are corroborated by other marginally-trusted certificates.
 * @GMIME_TRUST_FULL: We can rely on identity assertions made by this certificate.
 * @GMIME_TRUST_ULTIMATE: This certificate is an undeniable root of trust (e.g. normally, this is a certificate controlled by the user themselves).
 *
 * The trust level of a certificate.  Trust level tries to answer the
 * question: "How much is the user willing to rely on cryptographic
 * identity assertions made by the owner of this certificate?"
 * 
 * By way of comparison with web browser X.509 certificate validation
 * stacks, the certificate of a "Root CA" has @GMIME_TRUST_ULTIMATE,
 * while the certificate of an intermediate CA has @GMIME_TRUST_FULL,
 * and an end-entity certificate (e.g., with CA:FALSE set) would have
 * @GMIME_TRUST_NEVER.
 **/
typedef enum {
	GMIME_TRUST_UNKNOWN   = 0,
	GMIME_TRUST_UNDEFINED = 1,
	GMIME_TRUST_NEVER     = 2,
	GMIME_TRUST_MARGINAL  = 3,
	GMIME_TRUST_FULL      = 4,
	GMIME_TRUST_ULTIMATE  = 5
} GMimeTrust;

/**
 * GMimeValidity:
 * @GMIME_VALIDITY_UNKNOWN: The User ID of the certificate is of unknown validity.
 * @GMIME_VALIDITY_UNDEFINED: The User ID of the certificate is undefined.
 * @GMIME_VALIDITY_NEVER: The User ID of the certificate is never to be treated as valid.
 * @GMIME_VALIDITY_MARGINAL: The User ID of the certificate is marginally valid (e.g. it has been certified by only one marginally-trusted party).
 * @GMIME_VALIDITY_FULL: The User ID of the certificate is fully valid.
 * @GMIME_VALIDITY_ULTIMATE: The User ID of the certificate is ultimately valid (i.e., usually the certificate belongs to the local user themselves).
 *
 * The validity level of a certificate's User ID.  Validity level
 * tries to answer the question: "How strongly do we believe that this
 * certificate belongs to the party it says it belongs to?"
 *
 * Note that some OpenPGP certificates have multiple User IDs, and
 * each User ID may have a different validity level (e.g. depending on
 * which third parties have certified which User IDs, and which third
 * parties the local user has chosen to trust).
 *
 * Similarly, an X.509 certificate can have multiple SubjectAltNames,
 * and each name may also have a different validity level (e.g. if the
 * issuing CA is bound by name constraints).
 *
 * Note that the GMime API currently only exposes the highest-validty
 * User ID for any given certificate.
 **/
typedef enum {
	GMIME_VALIDITY_UNKNOWN   = 0,
	GMIME_VALIDITY_UNDEFINED = 1,
	GMIME_VALIDITY_NEVER     = 2,
	GMIME_VALIDITY_MARGINAL  = 3,
	GMIME_VALIDITY_FULL      = 4,
	GMIME_VALIDITY_ULTIMATE  = 5
} GMimeValidity;

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
 * @user_id: The full User ID of the certificate.
 * @id_validity: the validity of the email address, name, and User ID.
 *
 * An object containing useful information about a certificate.
 **/
struct _GMimeCertificate {
	GObject parent_object;
	
	GMimePubKeyAlgo pubkey_algo;
	GMimeDigestAlgo digest_algo;
	GMimeTrust trust;
	char *issuer_serial;
	char *issuer_name;
	char *fingerprint;
	time_t created;
	time_t expires;
	char *keyid;
	char *email;
	char *name;
	char *user_id;
	GMimeValidity id_validity;
};

struct _GMimeCertificateClass {
	GObjectClass parent_class;
	
};


GType g_mime_certificate_get_type (void);

GMimeCertificate *g_mime_certificate_new (void);

void g_mime_certificate_set_trust (GMimeCertificate *cert, GMimeTrust trust);
GMimeTrust g_mime_certificate_get_trust (GMimeCertificate *cert);

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

void g_mime_certificate_set_user_id (GMimeCertificate *cert, const char *user_id);
const char *g_mime_certificate_get_user_id (GMimeCertificate *cert);

void g_mime_certificate_set_id_validity (GMimeCertificate *cert, GMimeValidity validity);
GMimeValidity g_mime_certificate_get_id_validity (GMimeCertificate *cert);

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
