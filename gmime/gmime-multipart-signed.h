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


#ifndef __GMIME_MULTIPART_SIGNED_H__
#define __GMIME_MULTIPART_SIGNED_H__

#include <gmime/gmime-multipart.h>
#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_SIGNED            (g_mime_multipart_signed_get_type ())


enum {
	GMIME_MULTIPART_SIGNED_CONTENT,
	GMIME_MULTIPART_SIGNED_SIGNATURE
};


/**
 * GMimeMultipartSigned:
 *
 * A multipart/signed MIME part.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMultipartSigned, g_mime_multipart_signed, GMIME, MULTIPART_SIGNED, GMimeMultipart)

struct _GMimeMultipartSignedClass {
	GMimeMultipartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMultipartSigned *g_mime_multipart_signed_new (void);

GMimeMultipartSigned *g_mime_multipart_signed_sign (GMimeCryptoContext *ctx, GMimeEntity *entity,
						    const char *userid, GError **err);

GMimeSignatureList *g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, GMimeVerifyFlags flags, GError **err);

G_END_DECLS

#endif /* __GMIME_MULTIPART_SIGNED_H__ */
