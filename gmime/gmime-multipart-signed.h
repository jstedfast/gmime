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


#ifndef __GMIME_MULTIPART_SIGNED_H__
#define __GMIME_MULTIPART_SIGNED_H__

#include <gmime/gmime-multipart.h>
#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_SIGNED            (g_mime_multipart_signed_get_type ())
#define GMIME_MULTIPART_SIGNED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MULTIPART_SIGNED, GMimeMultipartSigned))
#define GMIME_MULTIPART_SIGNED_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART_SIGNED, GMimeMultipartSignedClass))
#define GMIME_IS_MULTIPART_SIGNED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MULTIPART_SIGNED))
#define GMIME_IS_MULTIPART_SIGNED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART_SIGNED))
#define GMIME_MULTIPART_SIGNED_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MULTIPART_SIGNED, GMimeMultipartSignedClass))

typedef struct _GMimeMultipartSigned GMimeMultipartSigned;
typedef struct _GMimeMultipartSignedClass GMimeMultipartSignedClass;

enum {
	GMIME_MULTIPART_SIGNED_CONTENT,
	GMIME_MULTIPART_SIGNED_SIGNATURE
};


/**
 * GMimeMultipartSigned:
 * @parent_object: parent #GMimeMultipart
 *
 * A multipart/signed MIME part.
 **/
struct _GMimeMultipartSigned {
	GMimeMultipart parent_object;
	
};

struct _GMimeMultipartSignedClass {
	GMimeMultipartClass parent_class;
	
};


GType g_mime_multipart_signed_get_type (void);

GMimeMultipartSigned *g_mime_multipart_signed_new (void);

int g_mime_multipart_signed_sign (GMimeMultipartSigned *mps, GMimeObject *content,
				  GMimeCryptoContext *ctx, const char *userid,
				  GMimeDigestAlgo digest, GError **err);

GMimeSignatureList *g_mime_multipart_signed_verify (GMimeMultipartSigned *mps,
						    GMimeCryptoContext *ctx,
						    GError **err);

G_END_DECLS

#endif /* __GMIME_MULTIPART_SIGNED_H__ */
