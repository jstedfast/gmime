/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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


#ifndef __PGP_UTILS_H__
#define __PGP_UTILS_H__

#include <glib.h>
#include "gmime-exception.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef enum {
	PGP_TYPE_NONE,
	PGP_TYPE_PGP2,
	PGP_TYPE_PGP5,
	PGP_TYPE_GPG
} PgpType;

typedef enum {
	PGP_HASH_TYPE_NONE,
	PGP_HASH_TYPE_MD5,
	PGP_HASH_TYPE_SHA1
} PgpHashType;

typedef gchar* (*PgpPasswdFunc) (const gchar *prompt, gpointer data);

void pgp_init (const gchar *path, PgpType type, PgpPasswdFunc callback, gpointer data);

gboolean pgp_detect (const gchar *text);

gboolean pgp_sign_detect (const gchar *text);

gchar *pgp_decrypt (const gchar *ciphertext, gint *outlen, GMimeException *ex);

gchar *pgp_encrypt (const gchar *in, gint inlen, const GPtrArray *recipients,
		    gboolean sign, const gchar *userid, GMimeException *ex);

gchar *pgp_clearsign (const gchar *plaintext, const gchar *userid,
		      PgpHashType hash, GMimeException *ex);

gchar *pgp_sign (const gchar *in, gint inlen, const gchar *userid,
		 PgpHashType hash, GMimeException *ex);

gboolean pgp_verify (const gchar *in, gint inlen, const gchar *sigin,
		     gint siglen, GMimeException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PGP_UTILS_H__ */
