/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_PGP_CONTEXT_H__
#define __GMIME_PGP_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "gmime-cipher-context.h"

typedef enum {
	GMIME_PGP_TYPE_NONE,
	GMIME_PGP_TYPE_PGP2,
	GMIME_PGP_TYPE_PGP5,
	GMIME_PGP_TYPE_PGP6,
	GMIME_PGP_TYPE_GPG
} GMimePgpType;

typedef char * (*GMimePgpPassphraseFunc) (const char *prompt, gpointer user_data);

typedef struct _GMimePgpContext {
	GMimeCipherContext parent_object;
	
	GMimePgpType type;
	char *path;
	
	GMimePgpPassphraseFunc get_passwd;
	gpointer user_data;
	
} GMimePgpContext;

#define GMIME_PGP_CONTEXT_TYPE g_str_hash ("GMimePgpContext")
#define GMIME_IS_PGP_CONTEXT(ctx) (((GMimeObject *) ctx)->type == GMIME_PGP_CONTEXT_TYPE)
#define GMIME_PGP_CONTEXT(ctx)    ((GMimePgpContext *) ctx)


GMimePgpContext  *g_mime_pgp_context_new (GMimePgpType type, const char *path,
					  GMimePgpPassphraseFunc get_passwd,
					  gpointer user_data);

/* PGP routines */
#define g_mime_pgp_sign(c, u, h, i, o, e) g_mime_cipher_sign (GMIME_CIPHER_CONTEXT (c), u, h, i, o, e)

#define g_mime_pgp_clearsign(c, u, h, i, o, e) g_mime_cipher_clearsign (GMIME_CIPHER_CONTEXT (c), u, h, i, o, e)

#define g_mime_pgp_verify(c, i, s, e) g_mime_cipher_verify (GMIME_CIPHER_CONTEXT (c), GMIME_CIPHER_HASH_DEFAULT, i, s, e)

#define g_mime_pgp_encrypt(c, s, u, r, i, o, e) g_mime_cipher_encrypt (GMIME_CIPHER_CONTEXT (c), s, u, r, i, o, e)

#define g_mime_pgp_decrypt(c, i, o, e) g_mime_cipher_decrypt (GMIME_CIPHER_CONTEXT (c), i, o, e)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PGP_CONTEXT_H__ */
