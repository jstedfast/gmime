/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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


#ifndef __GMIME_CIPHER_CONTEXT_H__
#define __GMIME_CIPHER_CONTEXT_H__

#include <glib.h>
#include <glib-object.h>

#include <time.h>

#include <gmime/gmime-stream.h>
#include <gmime/gmime-session.h>

G_BEGIN_DECLS

#define GMIME_TYPE_CIPHER_CONTEXT            (g_mime_cipher_context_get_type ())
#define GMIME_CIPHER_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContext))
#define GMIME_CIPHER_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContextClass))
#define GMIME_IS_CIPHER_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_CIPHER_CONTEXT))
#define GMIME_IS_CIPHER_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CIPHER_CONTEXT))
#define GMIME_CIPHER_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContextClass))

typedef struct _GMimeCipherContext GMimeCipherContext;
typedef struct _GMimeCipherContextClass GMimeCipherContextClass;

typedef struct _GMimeSigner GMimeSigner;
typedef struct _GMimeSignatureValidity GMimeSignatureValidity;

typedef enum {
	GMIME_CIPHER_HASH_DEFAULT,
	GMIME_CIPHER_HASH_MD2,
	GMIME_CIPHER_HASH_MD5,
	GMIME_CIPHER_HASH_SHA1,
	GMIME_CIPHER_HASH_RIPEMD160,
	GMIME_CIPHER_HASH_TIGER192,
	GMIME_CIPHER_HASH_HAVAL5160
} GMimeCipherHash;

struct _GMimeCipherContext {
	GObject parent_object;
	
	GMimeSession *session;
	
	/* these must be set by the subclass */
	const char *sign_protocol;
	const char *encrypt_protocol;
	const char *key_protocol;
};

struct _GMimeCipherContextClass {
	GObjectClass parent_class;
	
	GMimeCipherHash          (* hash_id)     (GMimeCipherContext *ctx, const char *hash);
	
	const char *             (* hash_name)   (GMimeCipherContext *ctx, GMimeCipherHash hash);
	
	int                      (* sign)        (GMimeCipherContext *ctx, const char *userid,
						  GMimeCipherHash hash, GMimeStream *istream,
						  GMimeStream *ostream, GError **err);
	
	GMimeSignatureValidity * (* verify)      (GMimeCipherContext *ctx, GMimeCipherHash hash,
						  GMimeStream *istream, GMimeStream *sigstream,
						  GError **err);
	
	int                      (* encrypt)     (GMimeCipherContext *ctx, gboolean sign,
						  const char *userid, GPtrArray *recipients,
						  GMimeStream *istream, GMimeStream *ostream,
						  GError **err);
	
	int                      (* decrypt)     (GMimeCipherContext *ctx, GMimeStream *istream,
						  GMimeStream *ostream, GError **err);
	
	int                      (* import_keys) (GMimeCipherContext *ctx, GMimeStream *istream,
						  GError **err);
	
	int                      (* export_keys) (GMimeCipherContext *ctx, GPtrArray *keys,
						  GMimeStream *ostream, GError **err);
};


GType g_mime_cipher_context_get_type (void);


/* hash routines */
GMimeCipherHash      g_mime_cipher_hash_id (GMimeCipherContext *ctx, const char *hash);

const char *         g_mime_cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

/* cipher routines */
int                  g_mime_cipher_sign (GMimeCipherContext *ctx, const char *userid,
					 GMimeCipherHash hash, GMimeStream *istream,
					 GMimeStream *ostream, GError **err);

GMimeSignatureValidity *g_mime_cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					      GMimeStream *istream, GMimeStream *sigstream,
					      GError **err);

int                  g_mime_cipher_encrypt (GMimeCipherContext *ctx, gboolean sign,
					    const char *userid, GPtrArray *recipients,
					    GMimeStream *istream, GMimeStream *ostream,
					    GError **err);

int                  g_mime_cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
					    GMimeStream *ostream, GError **err);

/* key/certificate routines */
int                  g_mime_cipher_import_keys (GMimeCipherContext *ctx, GMimeStream *istream,
						GError **err);

int                  g_mime_cipher_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
						GMimeStream *ostream, GError **err);



/* signature status structures and functions */
typedef enum {
	GMIME_SIGNER_TRUST_NONE,
	GMIME_SIGNER_TRUST_NEVER,
	GMIME_SIGNER_TRUST_UNDEFINED,
	GMIME_SIGNER_TRUST_MARGINAL,
	GMIME_SIGNER_TRUST_FULLY,
	GMIME_SIGNER_TRUST_ULTIMATE
} GMimeSignerTrust;

typedef enum {
	GMIME_SIGNER_STATUS_NONE,
	GMIME_SIGNER_STATUS_GOOD,
	GMIME_SIGNER_STATUS_BAD,
	GMIME_SIGNER_STATUS_ERROR
} GMimeSignerStatus;

typedef enum {
	GMIME_SIGNER_ERROR_NONE,
	GMIME_SIGNER_ERROR_EXPSIG     = (1 << 0),  /* expire signature */
	GMIME_SIGNER_ERROR_NO_PUBKEY  = (1 << 1),  /* no public key */
	GMIME_SIGNER_ERROR_EXPKEYSIG  = (1 << 2),  /* expired key */
	GMIME_SIGNER_ERROR_REVKEYSIG  = (1 << 3)   /* revoked key */
} GMimeSignerError;

struct _GMimeSigner {
	struct _GMimeSigner *next;
	unsigned int status:2;    /* GMimeSignerStatus */
	unsigned int errors:4;    /* bitfield of GMimeSignerError's */
	unsigned int trust:3;     /* GMimeSignerTrust */
	unsigned int unused:23;   /* unused expansion bits */
	time_t sig_created;
	time_t sig_expire;
	char *fingerprint;
	char *keyid;
	char *name;
};


GMimeSigner *g_mime_signer_new (void);
void         g_mime_signer_free (GMimeSigner *signer);


typedef enum {
	GMIME_SIGNATURE_STATUS_NONE,
	GMIME_SIGNATURE_STATUS_GOOD,
	GMIME_SIGNATURE_STATUS_BAD,
	GMIME_SIGNATURE_STATUS_UNKNOWN
} GMimeSignatureStatus;

struct _GMimeSignatureValidity {
	GMimeSignatureStatus status;
	GMimeSigner *signers;
	char *details;
};


GMimeSignatureValidity *g_mime_signature_validity_new (void);
void                    g_mime_signature_validity_free (GMimeSignatureValidity *validity);

GMimeSignatureStatus    g_mime_signature_validity_get_status (GMimeSignatureValidity *validity);
void                    g_mime_signature_validity_set_status (GMimeSignatureValidity *validity, GMimeSignatureStatus status);

const char             *g_mime_signature_validity_get_details (GMimeSignatureValidity *validity);
void                    g_mime_signature_validity_set_details (GMimeSignatureValidity *validity, const char *details);

const GMimeSigner      *g_mime_signature_validity_get_signers (GMimeSignatureValidity *validity);
void                    g_mime_signature_validity_add_signer  (GMimeSignatureValidity *validity, GMimeSigner *signer);


#ifndef GMIME_DISABLE_DEPRECATED

/* for backward compatability */
typedef struct _GMimeSignatureValidity GMimeCipherValidity;

GMimeCipherValidity *g_mime_cipher_validity_new (void);

void                 g_mime_cipher_validity_init (GMimeCipherValidity *validity);

gboolean             g_mime_cipher_validity_get_valid (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_set_valid (GMimeCipherValidity *validity, gboolean valid);

const char          *g_mime_cipher_validity_get_description (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_set_description (GMimeCipherValidity *validity,
							     const char *description);

void                 g_mime_cipher_validity_clear (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_free (GMimeCipherValidity *validity);

#endif /* GMIME_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GMIME_CIPHER_CONTEXT_H__ */
