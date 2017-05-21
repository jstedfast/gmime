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


#ifndef __GMIME_SIGNATURE_H__
#define __GMIME_SIGNATURE_H__

#include <gmime/gmime-certificate.h>

G_BEGIN_DECLS

#define GMIME_TYPE_SIGNATURE                  (g_mime_signature_get_type ())
#define GMIME_SIGNATURE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_SIGNATURE, GMimeSignature))
#define GMIME_SIGNATURE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_SIGNATURE, GMimeSignatureClass))
#define GMIME_IS_SIGNATURE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_SIGNATURE))
#define GMIME_IS_SIGNATURE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_SIGNATURE))
#define GMIME_SIGNATURE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_SIGNATURE, GMimeSignatureClass))

#define GMIME_TYPE_SIGNATURE_LIST             (g_mime_signature_list_get_type ())
#define GMIME_SIGNATURE_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_SIGNATURE_LIST, GMimeSignatureList))
#define GMIME_SIGNATURE_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_SIGNATURE_LIST, GMimeSignatureListClass))
#define GMIME_IS_SIGNATURE_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_SIGNATURE_LIST))
#define GMIME_IS_SIGNATURE_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_SIGNATURE_LIST))
#define GMIME_SIGNATURE_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_SIGNATURE_LIST, GMimeSignatureListClass))


typedef struct _GMimeSignature GMimeSignature;
typedef struct _GMimeSignatureClass GMimeSignatureClass;

typedef struct _GMimeSignatureList GMimeSignatureList;
typedef struct _GMimeSignatureListClass GMimeSignatureListClass;


/**
 * GMimeSignatureStatus:
 * @GMIME_SIGNATURE_STATUS_VALID: The signature is fully valid.
 * @GMIME_SIGNATURE_STATUS_GREEN: The signature is good.
 * @GMIME_SIGNATURE_STATUS_RED: The signature is bad.
 * @GMIME_SIGNATURE_STATUS_KEY_REVOKED: The key has been revoked.
 * @GMIME_SIGNATURE_STATUS_KEY_EXPIRED: The key has expired.
 * @GMIME_SIGNATURE_STATUS_SIG_EXPIRED: The signature has expired.
 * @GMIME_SIGNATURE_STATUS_KEY_MISSING: Can't verify due to missing key.
 * @GMIME_SIGNATURE_STATUS_CRL_MISSING: CRL not available.
 * @GMIME_SIGNATURE_STATUS_CRL_TOO_OLD: Available CRL is too old.
 * @GMIME_SIGNATURE_STATUS_BAD_POLICY: A policy was not met.
 * @GMIME_SIGNATURE_STATUS_SYS_ERROR: A system error occurred.
 * @GMIME_SIGNATURE_STATUS_TOFU_CONFLICT: Tofu conflict detected.
 *
 * A value representing the signature status bit flags for a particular
 * #GMimeSignature.
 **/
typedef enum {
	GMIME_SIGNATURE_STATUS_VALID         = 0x0001,
	GMIME_SIGNATURE_STATUS_GREEN         = 0x0002,
	GMIME_SIGNATURE_STATUS_RED           = 0x0004,
	GMIME_SIGNATURE_STATUS_KEY_REVOKED   = 0x0010,
	GMIME_SIGNATURE_STATUS_KEY_EXPIRED   = 0x0020,
	GMIME_SIGNATURE_STATUS_SIG_EXPIRED   = 0x0040,
	GMIME_SIGNATURE_STATUS_KEY_MISSING   = 0x0080,
	GMIME_SIGNATURE_STATUS_CRL_MISSING   = 0x0100,
	GMIME_SIGNATURE_STATUS_CRL_TOO_OLD   = 0x0200,
	GMIME_SIGNATURE_STATUS_BAD_POLICY    = 0x0400,
	GMIME_SIGNATURE_STATUS_SYS_ERROR     = 0x0800,
	GMIME_SIGNATURE_STATUS_TOFU_CONFLICT = 0x1000
} GMimeSignatureStatus;


/**
 * GMIME_SIGNATURE_STATUS_ERROR_MASK:
 *
 * A convenience macro for masking out the non-error bit flags.
 **/
#define GMIME_SIGNATURE_STATUS_ERROR_MASK ~(GMIME_SIGNATURE_STATUS_VALID | GMIME_SIGNATURE_STATUS_GREEN | GMIME_SIGNATURE_STATUS_RED)


/**
 * GMimeSignature:
 * @parent_object: parent #GObject
 * @status: A bitfield of #GMimeSignatureStatus values.
 * @cert: The #GMimeCertificate used in the signature.
 * @created: The creation date of the signature.
 * @expires: The expiration date of the signature.
 *
 * An object containing useful information about a signature.
 **/
struct _GMimeSignature {
	GObject parent_object;
	
	GMimeSignatureStatus status;
	GMimeCertificate *cert;
	time_t created;
	time_t expires;
};

struct _GMimeSignatureClass {
	GObjectClass parent_class;
	
};


GType g_mime_signature_get_type (void);

GMimeSignature *g_mime_signature_new (void);

void g_mime_signature_set_certificate (GMimeSignature *sig, GMimeCertificate *cert);
GMimeCertificate *g_mime_signature_get_certificate (GMimeSignature *sig);

void g_mime_signature_set_status (GMimeSignature *sig, GMimeSignatureStatus status);
GMimeSignatureStatus g_mime_signature_get_status (GMimeSignature *sig);

void g_mime_signature_set_created (GMimeSignature *sig, time_t created);
time_t g_mime_signature_get_created (GMimeSignature *sig);

void g_mime_signature_set_expires (GMimeSignature *sig, time_t expires);
time_t g_mime_signature_get_expires (GMimeSignature *sig);


/**
 * GMimeSignatureList:
 * @parent_object: parent #GObject
 *
 * A collection of #GMimeSignature objects.
 **/
struct _GMimeSignatureList {
	GObject parent_object;
	
	/* < private > */
	GPtrArray *array;
};

struct _GMimeSignatureListClass {
	GObjectClass parent_class;
	
};


GType g_mime_signature_list_get_type (void);

GMimeSignatureList *g_mime_signature_list_new (void);

int g_mime_signature_list_length (GMimeSignatureList *list);

void g_mime_signature_list_clear (GMimeSignatureList *list);

int g_mime_signature_list_add (GMimeSignatureList *list, GMimeSignature *sig);
void g_mime_signature_list_insert (GMimeSignatureList *list, int index, GMimeSignature *sig);
gboolean g_mime_signature_list_remove (GMimeSignatureList *list, GMimeSignature *sig);
gboolean g_mime_signature_list_remove_at (GMimeSignatureList *list, int index);

gboolean g_mime_signature_list_contains (GMimeSignatureList *list, GMimeSignature *sig);
int g_mime_signature_list_index_of (GMimeSignatureList *list, GMimeSignature *sig);

GMimeSignature *g_mime_signature_list_get_signature (GMimeSignatureList *list, int index);
void g_mime_signature_list_set_signature (GMimeSignatureList *list, int index, GMimeSignature *sig);

G_END_DECLS

#endif /* __GMIME_SIGNATURE_H__ */
