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
 * @GMIME_SIGNATURE_STATUS_GOOD: Good signature.
 * @GMIME_SIGNATURE_STATUS_ERROR: An error occurred.
 * @GMIME_SIGNATURE_STATUS_BAD: Bad signature.
 *
 * A value representing the signature status for a particular
 * #GMimeSignature.
 **/
typedef enum {
	GMIME_SIGNATURE_STATUS_GOOD,
	GMIME_SIGNATURE_STATUS_ERROR,
	GMIME_SIGNATURE_STATUS_BAD
} GMimeSignatureStatus;


/**
 * GMimeSignatureError:
 * @GMIME_SIGNATURE_ERROR_NONE: No error.
 * @GMIME_SIGNATURE_ERROR_EXPSIG: Expired signature.
 * @GMIME_SIGNATURE_ERROR_NO_PUBKEY: No public key found.
 * @GMIME_SIGNATURE_ERROR_EXPKEYSIG: Expired signature key.
 * @GMIME_SIGNATURE_ERROR_REVKEYSIG: Revoked signature key.
 * @GMIME_SIGNATURE_ERROR_UNSUPP_ALGO: Unsupported algorithm.
 *
 * Possible errors that a #GMimeSignature could have.
 **/
typedef enum {
	GMIME_SIGNATURE_ERROR_NONE        = 0,
	GMIME_SIGNATURE_ERROR_EXPSIG      = (1 << 0),  /* expired signature */
	GMIME_SIGNATURE_ERROR_NO_PUBKEY   = (1 << 1),  /* no public key */
	GMIME_SIGNATURE_ERROR_EXPKEYSIG   = (1 << 2),  /* expired key */
	GMIME_SIGNATURE_ERROR_REVKEYSIG   = (1 << 3),  /* revoked key */
	GMIME_SIGNATURE_ERROR_UNSUPP_ALGO = (1 << 4)   /* unsupported algorithm */
} GMimeSignatureError;


/**
 * GMimeSignature:
 * @parent_object: parent #GObject
 * @status: A #GMimeSignatureStatus.
 * @errors: A bitfield of #GMimeSignatureError values.
 * @cert: The #GMimeCertificate used in the signature.
 * @created: The creation date of the signature.
 * @expires: The expiration date of the signature.
 *
 * An object containing useful information about a signature.
 **/
struct _GMimeSignature {
	GObject parent_object;
	
	GMimeSignatureStatus status;
	GMimeSignatureError errors;
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

void g_mime_signature_set_errors (GMimeSignature *sig, GMimeSignatureError errors);
GMimeSignatureError g_mime_signature_get_errors (GMimeSignature *sig);

void g_mime_signature_set_created (GMimeSignature *sig, time_t created);
time_t g_mime_signature_get_created (GMimeSignature *sig);

void g_mime_signature_set_expires (GMimeSignature *sig, time_t expires);
time_t g_mime_signature_get_expires (GMimeSignature *sig);


/**
 * GMimeSignatureList:
 * @parent_object: parent #GObject
 * @array: An array of #GMimeSignature objects.
 *
 * A collection of #GMimeSignature objects.
 **/
struct _GMimeSignatureList {
	GObject parent_object;
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
