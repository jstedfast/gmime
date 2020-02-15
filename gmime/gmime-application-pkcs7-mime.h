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


#ifndef __GMIME_APPLICATION_PKCS7_MIME_H__
#define __GMIME_APPLICATION_PKCS7_MIME_H__

#include <glib.h>

#include <gmime/gmime-part.h>
#include <gmime/gmime-pkcs7-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_APPLICATION_PKCS7_MIME            (g_mime_application_pkcs7_mime_get_type ())
#define GMIME_APPLICATION_PKCS7_MIME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_APPLICATION_PKCS7_MIME, GMimeApplicationPkcs7Mime))
#define GMIME_APPLICATION_PKCS7_MIME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_APPLICATION_PKCS7_MIME, GMimeApplicationPkcs7MimeClass))
#define GMIME_IS_APPLICATION_PKCS7_MIME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_APPLICATION_PKCS7_MIME))
#define GMIME_IS_APPLICATION_PKCS7_MIME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_APPLICATION_PKCS7_MIME))
#define GMIME_APPLICATION_PKCS7_MIME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_APPLICATION_PKCS7_MIME, GMimeApplicationPkcs7MimeClass))

typedef struct _GMimeApplicationPkcs7Mime GMimeApplicationPkcs7Mime;
typedef struct _GMimeApplicationPkcs7MimeClass GMimeApplicationPkcs7MimeClass;

/**
 * GMimeSecureMimeType:
 * @GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA: The S/MIME content contains compressed data.
 * @GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA: The S/MIME content contains enveloped data.
 * @GMIME_SECURE_MIME_TYPE_SIGNED_DATA: The S/MIME content contains signed data.
 * @GMIME_SECURE_MIME_TYPE_CERTS_ONLY: The S/MIME content contains only certificates.
 * @GMIME_SECURE_MIME_TYPE_UNKNOWN: The S/MIME content is unknown.
 *
 * The S/MIME data type.
 **/
typedef enum _GMimeSecureMimeType {
	GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA,
	GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA,
	GMIME_SECURE_MIME_TYPE_SIGNED_DATA,
	GMIME_SECURE_MIME_TYPE_CERTS_ONLY,
	GMIME_SECURE_MIME_TYPE_UNKNOWN,
} GMimeSecureMimeType;

/**
 * GMimeApplicationPkcs7Mime:
 * @parent_object: parent #GMimePart object
 * @smime_type: The smime-type Content-Type parameter.
 *
 * An application/pkcs7-mime MIME part.
 **/
struct _GMimeApplicationPkcs7Mime {
	GMimePart parent_object;

	GMimeSecureMimeType smime_type;
};

struct _GMimeApplicationPkcs7MimeClass {
	GMimePartClass parent_class;
	
};


GType g_mime_application_pkcs7_mime_get_type (void);

GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_new (GMimeSecureMimeType type);

GMimeSecureMimeType g_mime_application_pkcs7_mime_get_smime_type (GMimeApplicationPkcs7Mime *pkcs7_mime);

/*GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_compress (GMimeObject *entity, GError **err);*/
/*GMimeObject *g_mime_application_pkcs7_mime_decompress (GMimeApplicationPkcs7Mime *pkcs7_mime);*/

GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_encrypt (GMimeObject *entity, GMimeEncryptFlags flags,
								  GPtrArray *recipients, GError **err);

GMimeObject *g_mime_application_pkcs7_mime_decrypt (GMimeApplicationPkcs7Mime *pkcs7_mime,
						    GMimeDecryptFlags flags, const char *session_key,
						    GMimeDecryptResult **result, GError **err);

GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_sign (GMimeObject *entity, const char *userid, GError **err);

GMimeSignatureList *g_mime_application_pkcs7_mime_verify (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeVerifyFlags flags,
							  GMimeObject **entity, GError **err);

G_END_DECLS

#endif /* __GMIME_APPLICATION_PKCS7_MIME_H__ */
