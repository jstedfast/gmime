/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_MULTIPART_ENCRYPTED_H__
#define __GMIME_MULTIPART_ENCRYPTED_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-multipart.h>
#include <gmime/gmime-cipher-context.h>

#define GMIME_TYPE_MULTIPART_ENCRYPTED            (g_mime_multipart_encrypted_get_type ())
#define GMIME_MULTIPART_ENCRYPTED(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncrypted))
#define GMIME_MULTIPART_ENCRYPTED_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncryptedClass))
#define GMIME_IS_MULTIPART_ENCRYPTED(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED))
#define GMIME_IS_MULTIPART_ENCRYPTED_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART_ENCRYPTED))
#define GMIME_MULTIPART_ENCRYPTED_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_MULTIPART_ENCRYPTED, GMimeMultipartEncryptedClass))

typedef struct _GMimeMultipartEncrypted GMimeMultipartEncrypted;
typedef struct _GMimeMultipartEncryptedClass GMimeMultipartEncryptedClass;

enum {
	GMIME_MULTIPART_ENCRYPTED_VERSION,
	GMIME_MULTIPART_ENCRYPTED_CONTENT
};

struct _GMimeMultipartEncrypted {
	GMimeMultipart parent_object;
	
	char *protocol;
	
	GMimeObject *decrypted;
};

struct _GMimeMultipartEncryptedClass {
	GMimeMultipartClass parent_class;
	
};


GType g_mime_multipart_encrypted_get_type (void);

GMimeMultipartEncrypted *g_mime_multipart_encrypted_new (void);

int g_mime_multipart_encrypted_encrypt (GMimeMultipartEncrypted *mpe, GMimeObject *content,
					GMimeCipherContext *ctx, GPtrArray *recipients,
					GError **err);

GMimeObject *g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *mpe,
						 GMimeCipherContext *ctx,
						 GError **err);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_MULTIPART_ENCRYPTED_H__ */
