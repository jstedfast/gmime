/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

G_BEGIN_DECLS

#define GMIME_TYPE_APPLICATION_PKCS7_MIME            (g_mime_application_pkcs7_mime_get_type ())


/**
 * GMimeSecureMimeType:
 * @GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA: The S/MIME content contains authenticated enveloped data.
 * @GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA: The S/MIME content contains compressed data.
 * @GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA: The S/MIME content contains enveloped data.
 * @GMIME_SECURE_MIME_TYPE_SIGNED_DATA: The S/MIME content contains signed data.
 * @GMIME_SECURE_MIME_TYPE_CERTS_ONLY: The S/MIME content contains only certificates.
 * @GMIME_SECURE_MIME_TYPE_UNKNOWN: The S/MIME content is unknown.
 *
 * The S/MIME data type.
 **/
typedef enum _GMimeSecureMimeType {
	GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA,
	GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA,
	GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA,
	GMIME_SECURE_MIME_TYPE_SIGNED_DATA,
	GMIME_SECURE_MIME_TYPE_CERTS_ONLY,
	GMIME_SECURE_MIME_TYPE_UNKNOWN,
} GMimeSecureMimeType;

/**
 * GMimeApplicationPkcs7Mime:
 *
 * An application/pkcs7-mime MIME part.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeApplicationPkcs7Mime, g_mime_application_pkcs7_mime, GMIME, APPLICATION_PKCS7_MIME, GMimePart)

struct _GMimeApplicationPkcs7MimeClass {
	GMimePartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_new (GMimeSecureMimeType type);

GMimeSecureMimeType g_mime_application_pkcs7_mime_get_smime_type (GMimeApplicationPkcs7Mime *pkcs7_mime);

GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_compress (GMimeEntity *entity, GError **err);
GMimeEntity *g_mime_application_pkcs7_mime_decompress (GMimeApplicationPkcs7Mime *pkcs7_mime,
						       size_t max_size, GError **err);

G_END_DECLS

#endif /* __GMIME_APPLICATION_PKCS7_MIME_H__ */
