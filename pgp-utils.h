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
#include <sys/types.h>
#include "gmime-exception.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef enum {
	PGP_TYPE_NONE,
	PGP_TYPE_PGP2,
	PGP_TYPE_PGP5,
	PGP_TYPE_PGP6,
	PGP_TYPE_GPG
} PgpType;

typedef enum {
	PGP_HASH_TYPE_NONE,
	PGP_HASH_TYPE_MD5,
	PGP_HASH_TYPE_SHA1
} PgpHashType;

typedef char *(*PgpPasswdFunc) (const char *prompt, gpointer data);

PgpType pgp_type_detect_from_path (const char *path);

void pgp_autodetect (char **pgp_path, PgpType *pgp_type);

void pgp_init (const char *path, PgpType type, PgpPasswdFunc callback, gpointer data);

gboolean pgp_detect (const char *text);

gboolean pgp_sign_detect (const char *text);

char *pgp_decrypt (const char *ciphertext, size_t cipherlen, size_t *outlen, GMimeException *ex);

char *pgp_encrypt (const char *in, size_t inlen, const GPtrArray *recipients,
		   gboolean sign, const char *userid, GMimeException *ex);

char *pgp_clearsign (const char *plaintext, const char *userid,
		     PgpHashType hash, GMimeException *ex);

char *pgp_sign (const char *in, size_t inlen, const char *userid,
		PgpHashType hash, GMimeException *ex);

gboolean pgp_verify (const char *in, size_t inlen, const char *sigin,
		     size_t siglen, GMimeException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PGP_UTILS_H__ */
