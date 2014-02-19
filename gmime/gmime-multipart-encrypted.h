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


#ifndef __GMIME_MULTIPART_ENCRYPTED_H__
#define __GMIME_MULTIPART_ENCRYPTED_H__

#include <gmime/gmime-multipart.h>
#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_ENCRYPTED            (g_mime_multipart_encrypted_get_type ())
#define GMIME_MULTIPART_ENCRYPTED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncrypted))
#define GMIME_MULTIPART_ENCRYPTED_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncryptedClass))
#define GMIME_IS_MULTIPART_ENCRYPTED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED))
#define GMIME_IS_MULTIPART_ENCRYPTED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART_ENCRYPTED))
#define GMIME_MULTIPART_ENCRYPTED_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncryptedClass))

typedef struct _GMimeMultipartEncrypted GMimeMultipartEncrypted;
typedef struct _GMimeMultipartEncryptedClass GMimeMultipartEncryptedClass;

enum {
	GMIME_MULTIPART_ENCRYPTED_VERSION,
	GMIME_MULTIPART_ENCRYPTED_CONTENT
};


/**
 * GMimeMultipartEncrypted:
 * @parent_object: parent #GMimeMultipart
 * @validity: a #GMimeSignatureValidity if the part has been decrypted or %NULL otherwise
 * @decrypted: cached decrypted MIME part
 *
 * A multipart/encrypted MIME part.
 **/
struct _GMimeMultipartEncrypted {
	GMimeMultipart parent_object;
	
};

struct _GMimeMultipartEncryptedClass {
	GMimeMultipartClass parent_class;
	
};


GType g_mime_multipart_encrypted_get_type (void);

GMimeMultipartEncrypted *g_mime_multipart_encrypted_new (void);

int g_mime_multipart_encrypted_encrypt (GMimeMultipartEncrypted *mpe, GMimeObject *content,
					GMimeCryptoContext *ctx, gboolean sign,
					const char *userid, GMimeDigestAlgo digest,
					GPtrArray *recipients, GError **err);

GMimeObject *g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *mpe,
						 GMimeCryptoContext *ctx,
						 GMimeDecryptResult **result,
						 GError **err);

G_END_DECLS

#endif /* __GMIME_MULTIPART_ENCRYPTED_H__ */
