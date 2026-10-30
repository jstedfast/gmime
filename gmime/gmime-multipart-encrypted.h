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


#ifndef __GMIME_MULTIPART_ENCRYPTED_H__
#define __GMIME_MULTIPART_ENCRYPTED_H__

#include <gmime/gmime-multipart.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_ENCRYPTED            (g_mime_multipart_encrypted_get_type ())


enum {
	GMIME_MULTIPART_ENCRYPTED_VERSION,
	GMIME_MULTIPART_ENCRYPTED_CONTENT
};


/**
 * GMimeMultipartEncrypted:
 *
 * A multipart/encrypted MIME part.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMultipartEncrypted, g_mime_multipart_encrypted, GMIME, MULTIPART_ENCRYPTED, GMimeMultipart)

struct _GMimeMultipartEncryptedClass {
	GMimeMultipartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMultipartEncrypted *g_mime_multipart_encrypted_new (void);

G_END_DECLS

#endif /* __GMIME_MULTIPART_ENCRYPTED_H__ */
